#pragma once

namespace PattyCore
{
    /*---------------*
     *    Message    *
     *---------------*/

    struct Message
    {
        using Id            = uint32_t;
        using Size          = uint32_t;
        using Payload       = std::vector<std::byte>;
        using Ptr           = UPtr<Message>;

        /*--------------*
         *    Header    *
         *--------------*/

        struct Header
        {
            Id      id = 0;
            Size    size = sizeof(Header);
        };

        Header      header;
        Payload     payload;

        size_t CalculateSize() const
        {
            return sizeof(Header) + payload.size();
        }
 
        // Push data to playload of message
        template<typename TData>
        friend Message& operator<<(Message& msg, const TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            const size_t offset = msg.payload.size();

            msg.payload.resize(offset + sizeof(TData));
            std::memcpy(msg.payload.data() + offset, &data, sizeof(TData));

            msg.header.size = static_cast<Message::Size>(msg.CalculateSize());

            return msg;
        }

        // Pop data from playload of message
        template<typename TData>
        friend Message& operator>>(Message& msg, TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            size_t offsetData = msg.payload.size() - sizeof(TData);
            
            std::memcpy(&data, msg.payload.data() + offsetData, sizeof(TData));
            msg.payload.resize(offsetData);

            msg.header.size = static_cast<Message::Size>(msg.CalculateSize());

            return msg;
        }

        friend std::ostream& operator<<(std::ostream& os, const Message& msg)
        {
            os << "id: " << msg.header.id <<  ", size: " << msg.header.size << "B";

            return os;
        }
    };

    /*--------------------*
     *    OwnedMessage    *
     *--------------------*/

    template<typename TOwner>
    struct OwnedMessage
    {
        using Buffer        = LockBuffer<OwnedMessage>;
        using OwnerPtr      = SPtr<TOwner>;

        OwnerPtr    owner;
        Message     msg;

        OwnedMessage() = default;

        OwnedMessage(OwnerPtr owner, Message&& msg)
            : owner(std::move(owner))
            , msg(std::move(msg))
        {}

        OwnedMessage(OwnerPtr owner, const Message& msg)
            : owner(std::move(owner))
            , msg(msg)
        {}

        friend std::ostream& operator<<(std::ostream& os, const OwnedMessage& ownedMsg)
        {
            os << *ownedMsg.owner << " " << ownedMsg.msg;

            return os;
        }
    };
}
