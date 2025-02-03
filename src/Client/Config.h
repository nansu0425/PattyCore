#pragma once

namespace Client::Config
{
    constexpr uint8_t numSocketThreads = 4;
    constexpr uint8_t numSessionThreads = 1;
    constexpr uint8_t numMessageThreads = 4;
    constexpr uint8_t numTaskThreads = 3;

    constexpr const char* host = "127.0.0.1";
    constexpr const char* service = "60000";
}
