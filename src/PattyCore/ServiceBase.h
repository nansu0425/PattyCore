#pragma once

#include <PattyCore/Session.h>

namespace PattyCore
{
    /*-------------------*
     *    ServiceBase    *
     *-------------------*/

    class ServiceBase
    {
    public:
        /*---------------*
         *    Threads    *
         *---------------*/

        class Threads
        {
        public:
            struct Info
            {
                uint8_t     nSocketThreads;
                uint8_t     nSessionThreads;
                uint8_t     nMessageThreads;
                uint8_t     nTaskThreads;
            };

        public:
            Threads(const Info& info)
                : _socketPool(info.nSocketThreads)
                , _sessionPool(info.nSessionThreads)
                , _messagePool(info.nMessageThreads)
                , _taskPool(info.nTaskThreads)
                , _socketGuard(asio::make_work_guard(_socketPool))
                , _sessionGuard(asio::make_work_guard(_sessionPool))
                , _messageGuard(asio::make_work_guard(_messagePool))
                , _taskGuard(asio::make_work_guard(_taskPool))
            {}

            void Stop()
            {
                _socketPool.stop();
                _sessionPool.stop();
                _messagePool.stop();
                _taskPool.stop();
            }

            void Join()
            {
                _socketPool.join();
                _sessionPool.join();
                _messagePool.join();
                _taskPool.join();
            }

            ThreadPool& SocketPool() { return _socketPool; }
            ThreadPool& SessionPool() { return _sessionPool; }
            ThreadPool& MessagePool() { return _messagePool; }
            ThreadPool& TaskPool() { return _taskPool; }

        private:
            ThreadPool      _socketPool;    // 소켓 입출력 스레드
            ThreadPool      _sessionPool;   // 세션 관리 스레드
            ThreadPool      _messagePool;   // 메시지 처리 스레드
            ThreadPool      _taskPool;      // 범용 비동기 작업 스레드

            WorkGuard       _socketGuard;
            WorkGuard       _sessionGuard;
            WorkGuard       _messageGuard;
            WorkGuard       _taskGuard;
        };

    protected:
        using OwnedMessage      = Session::OwnedMessage;

    public:
        ServiceBase(const Threads::Info& threadsInfo)
            : _threads(threadsInfo)
            , _sessionStrand(asio::make_strand(_threads.SessionPool()))
        {}

        virtual ~ServiceBase()
        {}

        void Stop()
        {
            _threads.Stop();
        }

        void Join()
        {
            _threads.Join();
        }

    protected:
        virtual void OnSessionRegistered(Session::Pointer pSession) {}
        virtual void OnSessionUnregistered(Session::Pointer pSession) {}
        virtual void OnMessageReceived(OwnedMessage ownedMessage) {}

        void CreateSession(Tcp::socket&& socket)
        {
            auto onSessionClosed = [this](const ErrorCode& error, Session::Pointer pSession)
                                   {
                                       OnSessionClosed(error, std::move(pSession));
                                   };

            auto onMessageReceived = [this](OwnedMessage&& ownedMessage)
                                     {
                                         DispatchReceivedMessage(std::move(ownedMessage));
                                     };

            Session::Pointer pSession = Session::Create(std::move(socket),
                                                        AssignId(),
                                                        std::move(onSessionClosed),
                                                        asio::make_strand(_threads.SocketPool()),
                                                        std::move(onMessageReceived));

            OnSessionCreated(std::move(pSession));
        }

        void BroadcastMessageAsync(Message&& message, Session::Pointer pIgnored = nullptr)
        {
            const Session::Id ignored = (pIgnored) ? pIgnored->GetId() : -1;

            asio::post(_sessionStrand,
                       [this, message = std::move(message), ignored]()
                       {
                           for (auto& pair : _sessionMap)
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

        void DispatchReceivedMessage(OwnedMessage&& ownedMessage)
        {
            asio::post(_threads.MessagePool(),
                       [this, ownedMessage = std::move(ownedMessage)]() mutable
                       {
                           OnMessageReceived(std::move(ownedMessage));
                       });
        }

        void OnSessionCreated(Session::Pointer pSession)
        {
            asio::post(_sessionStrand,
                       [this, pSession = std::move(pSession)]() mutable
                       {
                           RegisterSession(std::move(pSession));
                       });
        }

        void RegisterSession(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();

            assert(_sessionMap.count(id) == 0);
            _sessionMap[id] = std::move(pSession);

            asio::post(_threads.SessionPool(),
                       [this, pSession = _sessionMap[id]]() mutable
                       {
                           OnSessionRegistered(std::move(pSession));
                       });
        }

        void OnSessionClosed(const ErrorCode& error, Session::Pointer pSession)
        {
            if (error)
            {
                std::cerr << *pSession << " Failed to close session: " << error << "\n";
            }

            asio::post(_sessionStrand,
                       [this, pSession = std::move(pSession)]() mutable
                       {
                           UnregisterSession(std::move(pSession));
                       });
        }

        void UnregisterSession(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();

            assert(_sessionMap.count(id) == 1);
            _sessionMap.erase(id);

            asio::post(_threads.SessionPool(),
                       [this, pSession = std::move(pSession)]() mutable
                       {
                           OnSessionUnregistered(std::move(pSession));
                       });
        }

    protected:
        Threads             _threads;

        Session::Map        _sessionMap;
        Strand              _sessionStrand;

    };
}
