#pragma once

/*----------------*
 *    Standard    *
 *----------------*/

#include <cassert>
#include <memory>
#include <utility>
#include <queue>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <chrono>
#include <cstdint>
#include <cstddef>

/*------------*
 *    Asio    *
 *------------*/

#ifdef _WIN32
#define WINVER          0x0A00
#define _WIN32_WINNT    0x0A00
#endif // _WIN32

#define ASIO_STANDALONE
#include <asio.hpp>

/*-----------------*
 *    PattyCore    *
 *-----------------*/

#include <PattyCore/Types.hpp>
