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
            , _receiveTimer(_workers.timers)
            , _receiveLoop(0)
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
        virtual void OnReceiveLoopMeasured(const uint64_t receiveLoop) {}

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
                                                        asio::make_strand(_workers.ioHandlers),
                                                        *_receiveBuffers[AssignReceiveBufferIdx()]);

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

        size_t AssignReceiveBufferIdx() const
        {
            static std::atomic<size_t> idx = 0;

            while (true)
            {
                size_t curIdx = idx.load();
                const size_t nextIdx = (curIdx + 1) % _receiveBuffers.size();

                if (idx.compare_exchange_weak(curIdx, nextIdx))
                {
                    return nextIdx;
                }
            }
        }

        void Run(size_t nMessageHandlers)
        {
            WaitReceiveTimerAsync();
            ReceiveMessagesAsync(nMessageHandlers);
        }

        void ReceiveMessagesAsync(size_t nMessageHandlers)
        {
            _receiveBuffers.reserve(nMessageHandlers);

            for (size_t iReceiveBuffer = 0; iReceiveBuffer < nMessageHandlers; ++iReceiveBuffer)
            {
                _receiveBuffers.emplace_back(std::make_unique<OwnedMessage::Buffer>());

                asio::post(_workers.messageHandlers,
                           [this, iReceiveBuffer]()
                           {
                               ReceiveMessages(iReceiveBuffer);
                           });
            }
        }

        void ReceiveMessages(size_t iReceiveBuffer)
        {
            while (true)
            {
                OwnedMessage ownedMessage;

                if (_receiveBuffers[iReceiveBuffer]->Pop(ownedMessage))
                {
                    OnMessageReceived(std::move(ownedMessage));
                }

                _receiveLoop.fetch_add(1);
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

        void WaitReceiveTimerAsync()
        {
            _receiveTimer.expires_after(Seconds(1));
            _receiveTimer.async_wait([this](const ErrorCode& error)
                                         {
                                             OnReceiveTimerExpired(error);
                                         });
        }

        void OnReceiveTimerExpired(const ErrorCode& error)
        {
            if (error)
            {
                std::cerr << "[MESSAGE_LOOP] Failed to wait: " << error << "\n";
                return;
            }

            WaitReceiveTimerAsync();

            const uint64_t messageLoopCount = _receiveLoop.exchange(0);
            OnReceiveLoopMeasured(messageLoopCount);
        }

    protected:
        Workers                     _workers;

        SessionMap                  _sessions;
        Strand                      _sessionsStrand;

        ReceiveBufferVec            _receiveBuffers;
        Timer                       _receiveTimer;
        std::atomic<uint64_t>       _receiveLoop;

    };
}
