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
        using TickRate              = uint64_t;
        using OwnedMessage          = Session::OwnedMessage;

    private:
        using SessionMap = std::unordered_map<Session::Id, Session::Pointer>;

        struct Workers
        {
            ThreadPool      io;
            ThreadPool      control;
            ThreadPool      handler;
            ThreadPool      timer;

            WorkGuard       ioGuard;
            WorkGuard       controlGuard;
            WorkGuard       handlerGuard;
            WorkGuard       timerGuard;

            Workers(size_t nIo,
                    size_t nControl,
                    size_t nHandler,
                    size_t nTimer)
                : io(nIo)
                , control(nControl)
                , handler(nHandler)
                , timer(nTimer)
                , ioGuard(asio::make_work_guard(io))
                , controlGuard(asio::make_work_guard(control))
                , handlerGuard(asio::make_work_guard(handler))
                , timerGuard(asio::make_work_guard(timer))
            {}

            void Stop()
            {
                io.stop();
                control.stop();
                handler.stop();
                timer.stop();
            }

            void Join()
            {
                io.join();
                control.join();
                handler.join();
                timer.join();
            }
        };

    public:
        ServiceBase(size_t nIo,
                    size_t nControl,
                    size_t nHandler,
                    size_t nTimer)
            : _workers(nIo, nControl, nHandler, nTimer)
            , _sessionsStrand(asio::make_strand(_workers.control))
            , _tickRateTimer(_workers.timer)
            , _tickRate(0)
        {
            Run(nHandler);
        }

        virtual ~ServiceBase()
        {}

        void Stop()
        {
            _workers.Stop();
        }

        void Join()
        {
            _workers.Join();
        }

    protected:
        virtual void OnSessionRegistered(Session::Pointer pSession) {}
        virtual void OnSessionUnregistered(Session::Pointer pSession) {}
        virtual void HandleReceivedMessage(OwnedMessage ownedMessage) {}
        virtual void OnTickRateMeasured(const TickRate tickRate) {}

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
                                                        asio::make_strand(_workers.io),
                                                        _receiveBuffer);

            asio::post(_sessionsStrand,
                       [this, pSession = std::move(pSession)]() mutable
                       {
                           RegisterSession(std::move(pSession));
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

                               pair.second->SendAsync(Message(message));
                           }
                       });
        }

    private:
        Session::Id AssignId()
        {
            static std::atomic<Session::Id> id = 10000;
            Session::Id assignedId = id.fetch_add(1);

            return assignedId;
        }

        void Run(size_t nHandlerPool)
        {
            WaitTickRateTimerAsync();
            HandleReceivedMessagesAsync(nHandlerPool);
        }

        void HandleReceivedMessagesAsync(size_t nHandlerPool)
        {
            for (int i = 0; i < nHandlerPool; ++i)
            {
                asio::post(_workers.handler,
                           [this]()
                           {
                               HandleReceivedMessages();
                           });
            }
        }

        void HandleReceivedMessages()
        {
            while (true)
            {
                OwnedMessage ownedMessage;

                if (_receiveBuffer.Pop(ownedMessage))
                {
                    HandleReceivedMessage(std::move(ownedMessage));
                }

                _tickRate.fetch_add(1);
            }
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

            asio::post(_workers.control,
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

            asio::post(_workers.control,
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

            const TickRate tickRate = _tickRate.exchange(0);
            OnTickRateMeasured(tickRate);
        }

    protected:
        Workers                     _workers;

        SessionMap                  _sessions;
        Strand                      _sessionsStrand;

        Timer                       _tickRateTimer;
        std::atomic<TickRate>       _tickRate;

        OwnedMessage::Buffer        _receiveBuffer;

    };
}
