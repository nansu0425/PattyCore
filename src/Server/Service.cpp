#include "Pch.h"
#include "Service.h"
#include "MessageId.h"
#include <Client/MessageId.h>

namespace Server
{
    Service::Service(const Threads::Info& threadsInfo, uint16_t port)
        : ServerServiceBase(threadsInfo, port)
        , _secondTimer(_threads.TaskPool())
    {
        WaitSecondAsync();
    }

    void Service::OnMessageReceived(OwnedMessage ownedMessage)
    {
        Client::MessageId messageId =
            static_cast<Client::MessageId>(ownedMessage.message.header.id);

        switch (messageId)
        {
        case Client::MessageId::Ping:
            HandlePing(std::move(ownedMessage.pOwner));
            break;
        default:
            break;
        }

        _nMessagesHandled.fetch_add(1);
    }

    void Service::HandlePing(Session::Pointer pSession)
    {
        Message message;
        message.header.id = static_cast<Message::Id>(MessageId::Ping);

        pSession->SendAsync(std::move(message));
    }

    void Service::WaitSecondAsync()
    {
        _secondTimer.expires_after(Seconds(1));
        _secondTimer.async_wait([this](const ErrorCode& error)
                                {
                                    OnSecondElapsed(error);
                                });
    }

    void Service::OnSecondElapsed(const ErrorCode& error)
    {
        if (error)
        {
            std::cerr << "[SERVER] Failed to wait a second: " << error << "\n";
            return;
        }

        const uint32_t nMessagesHandled = _nMessagesHandled.exchange(0);
        WaitSecondAsync();

        std::cout << "[SERVER] The number of messages handled per second: " << nMessagesHandled << "\n";
    }
}
