#pragma once

namespace PattyCore
{
    /*----------------*
     *    Standard    *
     *----------------*/

    using ErrCode           = std::error_code;

    using TimePoint         = std::chrono::steady_clock::time_point;
    using Seconds           = std::chrono::seconds;
    using Milliseconds      = std::chrono::milliseconds;
    using Microseconds      = std::chrono::microseconds;
    using Nanoseconds       = std::chrono::nanoseconds;

    using Mutex             = std::mutex;
    using MutexLockGrd      = std::lock_guard<Mutex>;
    using MutexULock        = std::unique_lock<Mutex>;

    using SMutex            = std::shared_mutex;
    using SMutexULock       = std::unique_lock<SMutex>;
    using SMutexSLock       = std::shared_lock<SMutex>;

    template<typename T>
    using UPtr              = std::unique_ptr<T>;
    template<typename T>
    using SPtr              = std::shared_ptr<T>;
    template<typename T>
    using WPtr              = std::weak_ptr<T>;

    /*------------*
     *    Asio    *
     *------------*/

    using ThreadPool        = asio::thread_pool;
    using WorkGrd           = asio::executor_work_guard<ThreadPool::executor_type>;
    using Strand            = asio::strand<ThreadPool::executor_type>;
    using Tcp               = asio::ip::tcp;
    using Endpoints         = asio::ip::basic_resolver_results<Tcp>;
    using Timer             = asio::steady_timer;
}
