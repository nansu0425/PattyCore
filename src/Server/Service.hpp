#pragma once

#include <Server/MessageId.hpp>
#include <Client/MessageId.hpp>

namespace Server
{
    /*---------------*
     *    Service    *
     *---------------*/

    class Service : public ServerServiceBase
    {
    public:
        Service(size_t nWorkers,
                uint16_t port)
            : ServerServiceBase(nWorkers, port)
        {}

    protected:
        virtual void HandleReceivedMessage(OwnedMessage ownedMessage) override
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
        }

        virtual void OnTickRateMeasured(const TickRate tickRate) override
        {
            std::cout << "[SERVER] Tick rate: " << tickRate << "hz\n";
        }

    private:
        void HandlePing(Session::Pointer pSession)
        {
            Message message;
            message.header.id = static_cast<Message::Id>(MessageId::Ping);

            SendMessageAsync(std::move(pSession),
                             std::move(message));
        }

    };
}
