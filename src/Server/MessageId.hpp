#pragma once

#include <Server/Include.hpp>

namespace Server
{
    enum class MessageId : PattyNet::Message::Id
    {
        Accept = 500,
        Deny,
        Echo,
        Send,
        Broadcast,
    };
}
