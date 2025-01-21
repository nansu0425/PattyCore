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
        ServiceBase(size_t nWorkers)
            : _workers(nWorkers)
            , _workGuard(asio::make_work_guard(_workers))
            , _sessionsStrand(asio::make_strand(_workers))
            , _tickRateTimer(_workers)
            , _tickRate(0)
        {}

        virtual ~ServiceBase()
        {}

        void StopWorkers()
        {
            _workers.stop();
        }

        void JoinWorkers()
        {
            _workers.join();
        }

    protected:
        virtual void OnSessionRegistered(Session::Pointer pSession) {}
        virtual void OnSessionUnregistered(Session::Pointer pSession) {}
        virtual void HandleReceivedMessage(OwnedMessage ownedMessage) {}
        virtual void OnTickRateMeasured(const TickRate tickRate) {}

        void Run()
        {
            WaitTickRateTimerAsync();
            FetchReceivedMessagesAsync();
        }

        void CreateSession(Tcp::socket&& socket)
        {
            auto onSessionClosed = [this](const ErrorCode& error,
                                          Session::Pointer pSession) mutable
                                   {
                                       OnSessionClosed(error, 
                                                       std::move(pSession));
                                   };

            Session::Pointer pSession = Session::Create(_workers,
                                                        std::move(socket),
                                                        AssignId(),
                                                        std::move(onSessionClosed));

            asio::post(_sessionsStrand,
                       [this, pSession = std::move(pSession)]() mutable
                       {
                           RegisterSession(std::move(pSession));
                       });
        }

        void SendMessageAsync(OwnedMessage&& ownedMessage)
        {
            _sendBuffer.Push(std::move(ownedMessage));
        }

        void SendMessageAsync(Session::Pointer pOwner, Message&& message)
        {
            _sendBuffer.Emplace(std::move(pOwner),
                                std::move(message));
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

                               OwnedMessage ownedMessage(pair.second, message);
                               _sendBuffer.Push(std::move(ownedMessage));
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

        void FetchReceivedMessagesAsync()
        {
            asio::post(_sessionsStrand,
                       [this]()
                       {
                           FetchReceivedMessages();
                       });
        }

        void FetchReceivedMessages()
        {
            for (auto& pair : _sessions)
            {
                pair.second->Fetch(_receiveBuffer);
            }

            HandleReceivedMessagesAsync();
        }

        void HandleReceivedMessagesAsync()
        {
            asio::post([this]()
                       {
                           HandleReceivedMessages();
                       });
        }

        void HandleReceivedMessages()
        {
            OwnedMessage ownedMessage;

            while (_receiveBuffer.Pop(ownedMessage))
            {
                HandleReceivedMessage(std::move(ownedMessage));
            }

            assert(!_receiveBuffer.Pop(ownedMessage));

            DispatchSendMessagesAsync();
        }

        void DispatchSendMessagesAsync()
        {
            asio::post(_sessionsStrand,
                       [this]()
                       {
                           DispatchSendMessages();
                       });
        }

        void DispatchSendMessages()
        {
            std::queue<OwnedMessage> movedSendBuffer;
            _sendBuffer >> movedSendBuffer;

            while (!movedSendBuffer.empty())
            {
                OwnedMessage ownedMessage = std::move(movedSendBuffer.front());
                movedSendBuffer.pop();

                const Session::Id id = ownedMessage.pOwner->GetId();

                if (_sessions.count(id) == 1)
                {
                    _sessions[id]->Dispatch(std::move(ownedMessage));
                }
            }

            _tickRate.fetch_add(1);
            FetchReceivedMessagesAsync();
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

            asio::post([this, 
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

            asio::post([this, 
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

            const TickRate tickRate = _tickRate.exchange(0);
            OnTickRateMeasured(tickRate);
        }

    protected:
        ThreadPool                      _workers;
        WorkGuard                       _workGuard;
        SessionMap                      _sessions;
        Strand                          _sessionsStrand;

        Timer                           _tickRateTimer;
        std::atomic<TickRate>           _tickRate;

        OwnedMessage::Buffer            _sendBuffer;
        OwnedMessage::Buffer            _receiveBuffer;

    };
}
