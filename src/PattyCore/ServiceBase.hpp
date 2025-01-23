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
        using OwnedMessage          = Session::OwnedMessage;
        using SessionMap            = std::unordered_map<Session::Id, Session::Pointer>;
        using ReceiveBufferVec      = std::vector<std::unique_ptr<OwnedMessage::Buffer>>;

        struct Workers
        {
            ThreadPool      ioHandlers;
            ThreadPool      controllers;
            ThreadPool      messageHandlers;
            ThreadPool      timers;

            WorkGuard       ioHandlersGuard;
            WorkGuard       controllersGuard;
            WorkGuard       messageHandlersGuard;
            WorkGuard       timersGuard;

            Workers(size_t nIoHandlers,
                    size_t nControllers,
                    size_t nMessageHandlers,
                    size_t nTimers)
                : ioHandlers(nIoHandlers)
                , controllers(nControllers)
                , messageHandlers(nMessageHandlers)
                , timers(nTimers)
                , ioHandlersGuard(asio::make_work_guard(ioHandlers))
                , controllersGuard(asio::make_work_guard(controllers))
                , messageHandlersGuard(asio::make_work_guard(messageHandlers))
                , timersGuard(asio::make_work_guard(timers))
            {}

            void Stop()
            {
                ioHandlers.stop();
                controllers.stop();
                messageHandlers.stop();
                timers.stop();
            }

            void Join()
            {
                ioHandlers.join();
                controllers.join();
                messageHandlers.join();
                timers.join();
            }
        };

    public:
        ServiceBase(size_t nIoHandlers,
                    size_t nControllers,
                    size_t nMessageHandlers,
                    size_t nTimers)
            : _workers(nIoHandlers, 
                       nControllers, 
                       nMessageHandlers, 
                       nTimers)
            , _sessionsStrand(asio::make_strand(_workers.controllers))
            , _dispatchTimer(_workers.timers)
            , _dispatchCount(0)
        {
            Run(nMessageHandlers);
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
        virtual void OnMessageReceived(OwnedMessage ownedMessage) {}
        virtual void OnDispatchCountMeasured(const uint64_t dispatchCount) {}

        void CreateSession(Tcp::socket&& socket)
        {
            auto onSessionClosed = [this](const ErrorCode& error,
                                          Session::Pointer pSession) mutable
                                   {
                                       OnSessionClosed(error,
                                                       std::move(pSession));
                                   };

            auto onMessageReceived = [this](OwnedMessage&& ownedMessage)
                                     {
                                         DispatchReceivedMessage(std::move(ownedMessage));
                                     };

            Session::Pointer pSession = Session::Create(std::move(socket),
                                                        AssignId(),
                                                        std::move(onSessionClosed),
                                                        asio::make_strand(_workers.ioHandlers),
                                                        std::move(onMessageReceived));

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
        Session::Id AssignId() const
        {
            static std::atomic<Session::Id> id = 10000;
            Session::Id assignedId = id.fetch_add(1);

            return assignedId;
        }

        void Run(size_t nMessageHandlers)
        {
            WaitDispatchTimerAsync();
        }

        void DispatchReceivedMessage(OwnedMessage&& ownedMessage)
        {
            asio::post(_workers.messageHandlers,
                       [this, ownedMessage = std::move(ownedMessage)]() mutable
                       {
                           OnMessageReceived(std::move(ownedMessage));
                           _dispatchCount.fetch_add(1);
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

            asio::post(_workers.controllers,
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

            asio::post(_workers.controllers,
                       [this, 
                        pSession = std::move(pSession)]() mutable
                       {
                           OnSessionUnregistered(std::move(pSession));
                       });
        }

        void WaitDispatchTimerAsync()
        {
            _dispatchTimer.expires_after(Seconds(1));
            _dispatchTimer.async_wait([this](const ErrorCode& error)
                                         {
                                             OnDispatchTimerExpired(error);
                                         });
        }

        void OnDispatchTimerExpired(const ErrorCode& error)
        {
            if (error)
            {
                std::cerr << "[MESSAGE_LOOP] Failed to wait: " << error << "\n";
                return;
            }

            WaitDispatchTimerAsync();

            const uint64_t dispatchCount = _dispatchCount.exchange(0);
            OnDispatchCountMeasured(dispatchCount);
        }

    protected:
        Workers                     _workers;

        SessionMap                  _sessions;
        Strand                      _sessionsStrand;

        Timer                       _dispatchTimer;
        std::atomic<uint64_t>       _dispatchCount;

    };
}
