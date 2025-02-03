#include "Pch.h"
#include "Service.h"
#include "MessageId.h"
#include <Client/MessageId.h>

namespace Server
{
    using namespace std::chrono_literals;

    Service::Service(const ThreadPoolGroup::Info& info, uint16_t port)
        : ServerServiceBase(info, port)
        , mSecondTimer(mThreadPoolGroup.GetTaskGroup())
    {
        WaitSecondAsync();
    }

    void Service::OnMessageReceived(OwnedMessage ownedMsg)
    {
        Client::MessageId msgId = static_cast<Client::MessageId>(ownedMsg.msg.header.id);

        switch (msgId)
        {
        case Client::MessageId::Ping:
            HandlePing(std::move(ownedMsg.owner));
            break;
        }

        mNumMsgsHandled.fetch_add(1);
    }

    void Service::HandlePing(Session::Ptr session)
    {
        Message msg;
        msg.header.id = static_cast<Message::Id>(MessageId::Ping);

        session->SendAsync(std::move(msg));
    }

    void Service::WaitSecondAsync()
    {
        mSecondTimer.expires_after(1s);
        mSecondTimer.async_wait([this](const ErrCode& errCode)
                                {
                                    OnSecondElapsed(errCode);
                                });
    }

    void Service::OnSecondElapsed(const ErrCode& errCode)
    {
        if (errCode)
        {
            std::cerr << "[SERVER] Failed to wait a second: " << errCode << "\n";
            return;
        }

        const uint32_t numMsgsHandled = mNumMsgsHandled.exchange(0);
        WaitSecondAsync();

        std::cout << "[SERVER] The number of messages handled per second: " << numMsgsHandled << "\n";
    }
}
