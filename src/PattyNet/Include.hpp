#pragma once

#ifdef _WIN32
#define WINVER          0x0A00
#define _WIN32_WINNT    0x0A00
#endif // _WIN32

#define ASIO_STANDALONE
#include <asio.hpp>
