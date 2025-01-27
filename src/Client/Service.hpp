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
            using Pointer = std::unique_ptr<PingTimer>;
            using Map = std::unordered_map<Session::Id, Pointer>;

            Session::Id     id;
            Timer           timer;
            TimePoint       start;

            PingTimer(Session::Id id,
                      ThreadPool& timerWorkers)
                : id(id)
                , timer(timerWorkers)
            {}

            ~PingTimer()
            {
                std::cout << "[" << id << "] PingTimer destroyed\n";
            }
        };

    public:
        Service(const Threads::Info& threadsInfo)
            : ClientServiceBase(threadsInfo)
            , _pingTimerStrand(asio::make_strand(_threads.TaskPool()))
        {}

    protected:
        virtual void OnSessionRegistered(Session::Pointer pSession) override
        {
            asio::post(_pingTimerStrand,
                       [this, pSession = std::move(pSession)]()
                       {
                           const Session::Id id = pSession->GetId();
                           _pingTimerMap.emplace(id, std::make_unique<PingTimer>(id, _threads.TaskPool()));

                           Ping(std::move(pSession));
                       });

        }

        virtual void OnSessionUnregistered(Session::Pointer pSession) override
        {
            asio::post(_pingTimerStrand,
                       [this, pSession = std::move(pSession)]()
                       {
                           _pingTimerMap.erase(pSession->GetId());
                       });
        }

        virtual void OnMessageReceived(OwnedMessage ownedMessage) override
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
        void Ping(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();

            if (_pingTimerMap.count(id) == 0)
            {
                std::cerr << *pSession << " Ping error: non-existent PingTimer\n";
                return;
            }

            _pingTimerMap[id]->start = std::chrono::steady_clock::now();

            Message message;
            message.header.id = static_cast<Message::Id>(MessageId::Ping);

            pSession->SendAsync(std::move(message));
        }

        void HandlePing(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();

            TimePoint end = std::chrono::steady_clock::now();

            // pingTimerMap에 접근할 때 다른 스레드에 의해 pingTimerMap 상태가 바뀌면 read access 버그 발생 가능
            auto elapsed = std::chrono::duration_cast<MicroSeconds>(end - _pingTimerMap[id]->start);

            asio::post(_pingTimerStrand,
                       [this, pSession]() mutable
                       {
                           WaitPingTimerAsync(std::move(pSession));
                       });

            std::cout << *pSession << " Ping: " << elapsed.count() << "us\n";
        }

        void WaitPingTimerAsync(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();

            if (_pingTimerMap.count(id) == 0)
            {
                std::cerr << *pSession << " Ping error: non-existent PingTimer\n";
                return;
            }

            _pingTimerMap[id]->timer.expires_after(Seconds(1));
            _pingTimerMap[id]->timer.async_wait([this, pSession = std::move(pSession)]
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


        void PingAsync(Session::Pointer pSession)
        {
            asio::post(_pingTimerStrand,
                      [this, pSession = std::move(pSession)]()
                      {
                           Ping(std::move(pSession));
                      });
        }

    private:
        PingTimer::Map      _pingTimerMap;
        Strand              _pingTimerStrand;

    };
}
