#pragma once

#include <Client/Include.hpp>

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
