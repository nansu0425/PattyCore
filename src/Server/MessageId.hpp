#pragma once

#include <Server/Include.hpp>

namespace Server
{
    /*-----------------*
     *    MessageId    *
     *-----------------*/

    enum class MessageId : PattyCore::Message::Id
    {
        Accept = 500,
        Deny,
        Echo,
        Send,
        Broadcast,
    };
}
