#pragma once

namespace Client::Config
{
    constexpr size_t nIoHandlers = 4;
    constexpr size_t nControllers = 2;
    constexpr size_t nMessageHandlers = 4;
    constexpr size_t nTimers = 2;

    constexpr const char* host = "127.0.0.1";
    constexpr const char* service = "60000";
    constexpr size_t nConnects = 1000;
}
