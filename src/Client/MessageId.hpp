#pragma once

#include <Client/Include.hpp>

namespace Client
{
    /*-----------------*
     *    MessageId    *
     *-----------------*/

    enum class MessageId : PattyCore::Message::Id
    {
        Echo = 1000,
    };
}
