#pragma once

namespace Server::Config
{
    constexpr size_t nIoHandlers = 4;
    constexpr size_t nControllers = 1;
    constexpr size_t nMessageHandlers = 4;
    constexpr size_t nTimers = 1;

    constexpr size_t port = 60000;
}
