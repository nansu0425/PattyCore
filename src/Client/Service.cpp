#include "Pch.h"
#include "Service.h"
#include "MessageId.h"
#include <Server/MessageId.h>

namespace Client
{
    Service::Service(const Threads::Info& threadsInfo)
        : ClientServiceBase(threadsInfo)
        , _pingTimerStrand(asio::make_strand(_threads.TaskPool()))
    {}

    void Service::OnSessionRegistered(Session::Pointer pSession)
    {
        asio::post(_pingTimerStrand,
                   [this, pSession = std::move(pSession)]()
                   {
                       const Session::Id id = pSession->GetId();
                       _pingTimerMap.emplace(id, std::make_unique<PingTimer>(id, _threads.TaskPool()));

                       Ping(std::move(pSession));
                   });
    }

    void Service::OnSessionUnregistered(Session::Pointer pSession)
    {
        asio::post(_pingTimerStrand,
                   [this, pSession = std::move(pSession)]()
                   {
                       _pingTimerMap.erase(pSession->GetId());
                   });
    }

    void Service::OnMessageReceived(OwnedMessage ownedMessage)
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

    void Service::Ping(Session::Pointer pSession)
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

    void Service::HandlePing(Session::Pointer pSession)
    {
        const Session::Id id = pSession->GetId();

        TimePoint end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<MicroSeconds>(end - _pingTimerMap[id]->start);

        asio::post(_pingTimerStrand,
                   [this, pSession]() mutable
                   {
                       WaitPingTimerAsync(std::move(pSession));
                   });

        std::cout << *pSession << " Ping: " << elapsed.count() << "us\n";
    }

    void Service::WaitPingTimerAsync(Session::Pointer pSession)
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

    void Service::OnPingTimerExpired(const ErrorCode& error, Session::Pointer pSession)
    {
        if (error)
        {
            std::cerr << *pSession << " Failed to wait PingTimer: " << error << "\n";
            return;
        }

        PingAsync(std::move(pSession));
    }

    void Service::PingAsync(Session::Pointer pSession)
    {
        asio::post(_pingTimerStrand,
                  [this, pSession = std::move(pSession)]()
                  {
                      Ping(std::move(pSession));
                  });
    }

    Service::PingTimer::PingTimer(Session::Id id, ThreadPool& timerWorkers)
        : id(id)
        , timer(timerWorkers)
    {}

    Service::PingTimer::~PingTimer()
    {
        std::cout << "[" << id << "] PingTimer destroyed\n";
    }
}
