#pragma once

namespace Server::Config
{
    constexpr uint8_t numSocketThreads = 4;
    constexpr uint8_t numSessionThreads = 3;
    constexpr uint8_t numMessageThreads = 4;
    constexpr uint8_t numTaskThreads = 1;

    constexpr uint16_t port = 60000;
}
