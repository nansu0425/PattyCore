#include "Pch.h"
#include "Service.h"
#include "MessageId.h"
#include <Server/MessageId.h>

namespace Client
{
    using namespace std::chrono_literals;

    Service::Service(const ThreadPoolGroup::Info& info)
        : ClientServiceBase(info)
        , mPingTimerStrand(asio::make_strand(mThreadPoolGroup.GetTaskGroup()))
    {}

    void Service::OnSessionRegistered(Session::Ptr session)
    {
        asio::post(mPingTimerStrand,
                   [this, session = std::move(session)]()
                   {
                       const Session::Id id = session->GetId();
                       mPingTimerMap.emplace(id, std::make_unique<PingTimer>(id, mThreadPoolGroup.GetTaskGroup()));

                       Ping(std::move(session));
                   });
    }

    void Service::OnSessionUnregistered(Session::Ptr session)
    {
        asio::post(mPingTimerStrand,
                   [this, session = std::move(session)]()
                   {
                       mPingTimerMap.erase(session->GetId());
                   });
    }

    void Service::OnMessageReceived(OwnedMessage ownedMsg)
    {
        const Server::MessageId msgId = static_cast<Server::MessageId>(ownedMsg.msg.header.id);

        switch (msgId)
        {
        case Server::MessageId::Ping:
            HandlePing(std::move(ownedMsg.owner));
            break;
        }
    }

    void Service::Ping(Session::Ptr session)
    {
        const Session::Id id = session->GetId();

        if (mPingTimerMap.count(id) == 0)
        {
            std::cerr << *session << " Ping error: non-existent PingTimer\n";
            return;
        }

        mPingTimerMap[id]->start = std::chrono::steady_clock::now();

        Message msg;
        msg.header.id = static_cast<Message::Id>(MessageId::Ping);

        session->SendAsync(std::move(msg));
    }

    void Service::HandlePing(Session::Ptr session)
    {
        const Session::Id id = session->GetId();

        TimePoint end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<Microseconds>(end - mPingTimerMap[id]->start);

        asio::post(mPingTimerStrand,
                   [this, session]() mutable
                   {
                       WaitPingTimerAsync(std::move(session));
                   });

        std::cout << *session << " Ping: " << elapsed.count() << "us\n";
    }

    void Service::WaitPingTimerAsync(Session::Ptr session)
    {
        const Session::Id id = session->GetId();

        if (mPingTimerMap.count(id) == 0)
        {
            std::cerr << *session << " Ping error: non-existent PingTimer\n";
            return;
        }

        mPingTimerMap[id]->timer.expires_after(1s);
        mPingTimerMap[id]->timer.async_wait([this, session = std::move(session)]
                                            (const ErrCode& errCode) mutable
                                            {
                                                OnPingTimerExpired(errCode, std::move(session));
                                            });
    }

    void Service::OnPingTimerExpired(const ErrCode& errCode, Session::Ptr session)
    {
        if (errCode)
        {
            std::cerr << *session << " Failed to wait PingTimer: " << errCode << "\n";
            return;
        }

        PingAsync(std::move(session));
    }

    void Service::PingAsync(Session::Ptr session)
    {
        asio::post(mPingTimerStrand,
                   [this, session = std::move(session)]()
                   {
                       Ping(std::move(session));
                   });
    }

    Service::PingTimer::PingTimer(const Session::Id id, ThreadPool& taskGroup)
        : id(id)
        , timer(taskGroup)
    {}

    Service::PingTimer::~PingTimer()
    {
        std::cout << "[" << id << "] PingTimer destroyed\n";
    }
}
