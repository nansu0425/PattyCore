#pragma once

#include <Client/Include.hpp>

namespace Client
{
    /*-----------------*
     *    MessageId    *
     *-----------------*/

    enum class MessageId : PattyNet::Message::Id
    {
        Echo = 1000,
    };
}
