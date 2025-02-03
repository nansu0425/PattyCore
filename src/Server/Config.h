#pragma once

namespace Server::Config
{
    constexpr uint8_t nSocketThreads = 4;
    constexpr uint8_t nSessionThreads = 3;
    constexpr uint8_t nMessageThreads = 4;
    constexpr uint8_t nTaskThreads = 1;

    constexpr uint16_t port = 60000;
}
