#pragma once

#include <Client/MessageId.hpp>
#include <Server/MessageId.hpp>

namespace Client
{
    /*---------------*
     *    Service    *
     *---------------*/

    class Service : public ClientServiceBase
    {
    private:
        /*-----------------*
         *    PingTimer    *
         *-----------------*/

        struct PingTimer
        {
            using Pointer   = std::unique_ptr<PingTimer>;
            using Map       = std::unordered_map<Session::Id, Pointer>;

            Session::Id     id;
            Timer           timer;
            TimePoint       start;

            PingTimer(Session::Id id,
                      ThreadPool& workers)
                : id(id)
                , timer(workers)
            {}

            ~PingTimer()
            {
                std::cout << "[" << id << "] PingTimer destroyed\n";
            }
        };

    public:
        Service(size_t nWorkers,
                uint16_t nConnects)
            : ClientServiceBase(nWorkers,
                                nConnects)
        {}

    protected:
        virtual void OnSessionRegistered(Session::Pointer pSession) override
        {
            {
                UniqueSharedLock lock(_pingTimersLock);

                const Session::Id id = pSession->GetId();
                _pingTimers.emplace(id,
                                    std::make_unique<PingTimer>(id, _workers));
            }

            PingAsync(std::move(pSession));
        }

        virtual void OnSessionUnregistered(Session::Pointer pSession) override
        {
            UniqueSharedLock lock(_pingTimersLock);

            _pingTimers.erase(pSession->GetId());
        }

        virtual void HandleReceivedMessage(OwnedMessage ownedMessage) override
        {
            Server::MessageId messageId = 
                static_cast<Server::MessageId>(ownedMessage.message.header.id);

            switch (messageId)
            {
            case Server::MessageId::Ping:
                HandlePing(std::move(ownedMessage.pOwner));
                break;

            default:
                break;
            }
        }

    private:
        void PingAsync(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();

            {
                SharedLock lock(_pingTimersLock);

                if (_pingTimers.count(id) == 0)
                {
                    std::cerr << *pSession << " Ping error: non-existent PingTimer\n";
                    return;
                }

                _pingTimers[id]->start = std::chrono::steady_clock::now();
            }

            Message message;
            message.header.id = static_cast<Message::Id>(MessageId::Ping);

            SendMessageAsync(std::move(pSession),
                             std::move(message));
        }

        void HandlePing(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();

            TimePoint end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<MicroSeconds>(end - _pingTimers[id]->start);

            std::cout << *pSession << " Ping: " << elapsed.count() << "us\n";

            SharedLock lock(_pingTimersLock);

            if (_pingTimers.count(id) == 0)
            {
                std::cerr << *pSession << " Ping error: non-existent PingTimer\n";
                return;
            }

            _pingTimers[id]->timer.expires_after(Seconds(1));
            _pingTimers[id]->timer.async_wait([this, pSession = std::move(pSession)]
                                              (const ErrorCode& error) mutable
                                              {
                                                  OnPingTimerExpired(error, 
                                                                     std::move(pSession));
                                              });
        }

        void OnPingTimerExpired(const ErrorCode& error, Session::Pointer pSession)
        {
            if (error)
            {
                std::cerr << *pSession << " Failed to wait PingTimer: " << error << "\n";
                return;
            }

            PingAsync(std::move(pSession));
        }

    private:
        PingTimer::Map      _pingTimers;
        SharedMutex         _pingTimersLock;
    
    };
}
