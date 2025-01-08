#pragma once

#include <Client/Pch.hpp>

namespace Client
{
    enum class MessageId : PattyNet::Message::Id
    {
        Echo = 1000,
    };
}
