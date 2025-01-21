#pragma once

#include <PattyCore/Session.hpp>

namespace PattyCore
{
    /*-------------------*
     *    ServiceBase    *
     *-------------------*/

    class ServiceBase
    {
    protected:
        using TickRate              = uint32_t;
        using OwnedMessage          = Session::OwnedMessage;

    private:
        using SessionMap = std::unordered_map<Session::Id, Session::Pointer>;

    public:
        ServiceBase(size_t nIoPool,
                    size_t nControlPool,
                    size_t nHandlerPool,
                    size_t nTimerPool)
            : _ioPool(nIoPool)
            , _controlPool(nControlPool)
            , _handlerPool(nHandlerPool)
            , _timerPool(nTimerPool)
            , _sessionsStrand(asio::make_strand(_controlPool))
            , _tickRateTimer(_timerPool)
            , _tickRate(0)
        {}

        virtual ~ServiceBase()
        {}

        void Stop()
        {
            _ioPool.stop();
            _controlPool.stop();
            _handlerPool.stop();
            _timerPool.stop();
        }

        void Join()
        {
            _ioPool.join();
            _controlPool.join();
            _handlerPool.join();
            _timerPool.join();
        }

    protected:
        virtual void OnSessionRegistered(Session::Pointer pSession) {}
        virtual void OnSessionUnregistered(Session::Pointer pSession) {}
        virtual void HandleReceivedMessage(OwnedMessage ownedMessage) {}
        virtual void OnTickRateMeasured(const TickRate tickRate) {}

        void Run()
        {
            WaitTickRateTimerAsync();
            HandleReceivedMessagesAsync();
        }

        void CreateSessionAsync(Tcp::socket&& socket)
        {
            asio::post(_controlPool,
                       [this, socket = std::move(socket)]() mutable
                       {
                           CreateSession(std::move(socket));
                       });
        }

        void BroadcastMessageAsync(Message&& message, Session::Pointer pIgnored = nullptr)
        {
            const Session::Id ignored = (pIgnored) ? pIgnored->GetId() : -1;

            asio::post(_sessionsStrand,
                       [this, 
                        message = std::move(message),
                        ignored]()
                       {
                           for (auto& pair : _sessions)
                           {
                               if (pair.first == ignored)
                               {
                                   continue;
                               }

                               pair.second->SendAsync(pair.second, Message(message));
                           }
                       });
        }

    private:
        Session::Id AssignId()
        {
            static Session::Id id = 10000;
            Session::Id assignedId = id;
            ++id;

            return assignedId;
        }

        void HandleReceivedMessagesAsync()
        {
            asio::post(_controlPool,
                       [this]()
                       {
                           HandleReceivedMessages();
                       });
        }

        void HandleReceivedMessages()
        {
            OwnedMessage ownedMessage;

            while (_receiveBuffer.Pop(ownedMessage))
            {
                asio::post(_handlerPool,
                           [this, ownedMessage = std::move(ownedMessage)]() mutable
                           {
                               HandleReceivedMessage(std::move(ownedMessage));
                           });
            }

            _tickRate.fetch_add(1);
            HandleReceivedMessagesAsync();
        }

        void CreateSession(Tcp::socket&& socket)
        {
            auto onSessionClosed = [this](const ErrorCode& error,
                                          Session::Pointer pSession) mutable
                                   {
                                       OnSessionClosed(error,
                                                       std::move(pSession));
                                   };

            Session::Pointer pSession = Session::Create(std::move(socket),
                                                        AssignId(),
                                                        std::move(onSessionClosed),
                                                        asio::make_strand(_ioPool),
                                                        _receiveBuffer);

            asio::post(_sessionsStrand,
                       [this, pSession = std::move(pSession)]() mutable
                       {
                           RegisterSession(std::move(pSession));
                       });
        }

        void OnSessionClosed(const ErrorCode& error, Session::Pointer pSession)
        {
            if (error)
            {
                std::cerr << *pSession << " Failed to close session: " << error << "\n";
            }

            asio::post(_sessionsStrand,
                       [this,
                        pSession = std::move(pSession)]() mutable
                       {
                           UnregisterSession(std::move(pSession));
                       });
        }

        void RegisterSession(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();
            
            assert(_sessions.count(id) == 0);
            _sessions[id] = std::move(pSession);

            asio::post(_handlerPool,
                       [this, 
                        pSession = _sessions[id]]() mutable
                       {
                           OnSessionRegistered(std::move(pSession));
                       });
        }

        void UnregisterSession(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();

            assert(_sessions.count(id) == 1);
            _sessions.erase(id);

            asio::post(_handlerPool,
                       [this, 
                        pSession = std::move(pSession)]() mutable
                       {
                           OnSessionUnregistered(std::move(pSession));
                       });
        }

        void WaitTickRateTimerAsync()
        {
            _tickRateTimer.expires_after(Seconds(1));
            _tickRateTimer.async_wait([this](const ErrorCode& error)
                                      {
                                          OnTickRateTimerExpired(error);
                                      });
        }

        void OnTickRateTimerExpired(const ErrorCode& error)
        {
            if (error)
            {
                std::cerr << "[TICK_RATE_TIMER] Failed to wait: " << error << "\n";
                return;
            }

            WaitTickRateTimerAsync();

            asio::post(_handlerPool,
                       [this]()
                       {
                           const TickRate tickRate = _tickRate.exchange(0);
                           OnTickRateMeasured(tickRate);
                       });
        }

    protected:
        ThreadPool                      _ioPool;
        ThreadPool                      _controlPool;
        ThreadPool                      _handlerPool;
        ThreadPool                      _timerPool;

        SessionMap                      _sessions;
        Strand                          _sessionsStrand;

        Timer                           _tickRateTimer;
        std::atomic<TickRate>           _tickRate;

        OwnedMessage::Buffer            _receiveBuffer;

    };
}
