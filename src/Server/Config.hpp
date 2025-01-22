#pragma once

namespace Server::Config
{
    constexpr size_t nIoHandlers = 4;
    constexpr size_t nControllers = 2;
    constexpr size_t nMessageHandlers = 4;
    constexpr size_t nTimers = 2;

    constexpr size_t port = 60000;
}
