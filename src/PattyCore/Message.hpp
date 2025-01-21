#pragma once

#include <PattyCore/Include.hpp>

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
        using Buffer        = LockBuffer<Message>;

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
        friend Message& operator<<(Message& message, const TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            const size_t offset = message.payload.size();

            message.payload.resize(offset + sizeof(TData));
            std::memcpy(message.payload.data() + offset, &data, sizeof(TData));

            message.header.size = static_cast<Message::Size>(message.CalculateSize());

            return message;
        }

        // Pop data from playload of message
        template<typename TData>
        friend Message& operator>>(Message& message, TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            size_t offsetData = message.payload.size() - sizeof(TData);
            
            std::memcpy(&data, message.payload.data() + offsetData, sizeof(TData));
            message.payload.resize(offsetData);

            message.header.size = static_cast<Message::Size>(message.CalculateSize());

            return message;
        }

        friend std::ostream& operator<<(std::ostream& os, const Message& message)
        {
            os << "id: " << message.header.id <<  ", size: " << message.header.size << "B";

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
        using OwnerPointer  = std::shared_ptr<TOwner>;

        OwnerPointer    pOwner;
        Message         message;

        OwnedMessage() = default;

        OwnedMessage(OwnerPointer pOwner, Message&& message)
            : pOwner(std::move(pOwner))
            , message(std::move(message))
        {}

        OwnedMessage(OwnerPointer pOwner, const Message& message)
            : pOwner(std::move(pOwner))
            , message(message)
        {}

        friend std::ostream& operator<<(std::ostream& os, const OwnedMessage& ownedMessage)
        {
            os << *ownedMessage.pOwner << " " << ownedMessage.message;

            return os;
        }
    };
}
