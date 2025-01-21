#pragma once

#include <Server/Include.hpp>

namespace Server
{
    /*-----------------*
     *    MessageId    *
     *-----------------*/

    enum class MessageId : Message::Id
    {
        Ping = 500,
    };
}
