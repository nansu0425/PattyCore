#pragma once

#include <PattyCore/Include.hpp>

namespace PattyCore
{
    /*----------------*
     *    Standard    *
     *----------------*/

    using ErrorCode         = std::error_code;

    using TimePoint         = std::chrono::steady_clock::time_point;
    using Seconds           = std::chrono::seconds;
    using Milliseconds      = std::chrono::milliseconds;
    using MicroSeconds      = std::chrono::microseconds;
    using NanoSeconds       = std::chrono::nanoseconds;

    using Mutex             = std::mutex;
    using LockGuard         = std::lock_guard<Mutex>;
    using UniqueLock        = std::unique_lock<Mutex>;

    using SharedMutex       = std::shared_mutex;
    using UniqueSharedLock  = std::unique_lock<SharedMutex>;
    using SharedLock        = std::shared_lock<SharedMutex>;

    /*------------*
     *    Asio    *
     *------------*/

    using ThreadPool        = asio::thread_pool;
    using WorkGuard         = asio::executor_work_guard<ThreadPool::executor_type>;
    using Strand            = asio::strand<ThreadPool::executor_type>;
    using Tcp               = asio::ip::tcp;
    using Endpoints         = asio::ip::basic_resolver_results<Tcp>;
    using Timer             = asio::steady_timer;
}
