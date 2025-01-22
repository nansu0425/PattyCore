#pragma once

namespace Client::Config
{
    constexpr size_t nIoHandlers = 1;
    constexpr size_t nControllers = 1;
    constexpr size_t nMessageHandlers = 1;
    constexpr size_t nTimers = 1;

    constexpr const char* host = "127.0.0.1";
    constexpr const char* service = "60000";
}
