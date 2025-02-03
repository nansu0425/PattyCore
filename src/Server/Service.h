#pragma once

#include <Server/MessageId.h>
#include <Client/MessageId.h>

namespace Server
{
    /*---------------*
     *    Service    *
     *---------------*/

    class Service : public ServerServiceBase
    {
    public:
        Service(const Threads::Info& threadsInfo, uint16_t port)
            : ServerServiceBase(threadsInfo, port)
            , _secondTimer(_threads.TaskPool())
        {
            WaitSecondAsync();
        }

    protected:
        virtual void OnMessageReceived(OwnedMessage ownedMessage) override
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

    private:
        void HandlePing(Session::Pointer pSession)
        {
            Message message;
            message.header.id = static_cast<Message::Id>(MessageId::Ping);

            pSession->SendAsync(std::move(message));
        }

        void WaitSecondAsync()
        {
            _secondTimer.expires_after(Seconds(1));
            _secondTimer.async_wait([this](const ErrorCode& error)
                                    {
                                        OnSecondElapsed(error);
                                    });
        }

        void OnSecondElapsed(const ErrorCode& error)
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

    private:
        Timer                   _secondTimer;
        std::atomic<uint32_t>   _nMessagesHandled = 0;

    };
}
