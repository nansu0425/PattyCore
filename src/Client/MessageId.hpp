#pragma once

#include <Client/Include.hpp>

namespace PattyCore::Client
{
    /*-----------------*
     *    MessageId    *
     *-----------------*/

    enum class MessageId : PattyCore::Message::Id
    {
        Echo = 1000,
    };
}
