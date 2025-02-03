#pragma once

#include <Client/Include.h>

namespace Client
{
    /*-----------------*
     *    MessageId    *
     *-----------------*/

    enum class MessageId : Message::Id
    {
        Ping = 1000,
    };
}
