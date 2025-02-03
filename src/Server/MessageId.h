#pragma once

#include <Server/Include.h>

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
