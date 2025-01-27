#pragma once

namespace Client::Config
{
    constexpr uint8_t nSocketThreads = 4;
    constexpr uint8_t nSessionThreads = 1;
    constexpr uint8_t nMessageThreads = 4;
    constexpr uint8_t nTaskThreads = 3;

    constexpr const char* host = "127.0.0.1";
    constexpr const char* service = "60000";
}
