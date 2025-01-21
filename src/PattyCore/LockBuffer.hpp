#pragma once

#pragma once

#include <PattyCore/Include.hpp>

namespace PattyCore
{
    template<typename TItem>
    class LockBuffer
    {
    public:
        void Push(TItem&& item)
        {
            LockGuard lock(_mutex);

            _queue.push(std::move(item));
        }

        bool Pop(TItem& item)
        {
            LockGuard lock(_mutex);

            if (_queue.empty())
            {
                return false;
            }

            item = std::move(_queue.front());
            _queue.pop();

            return true;
        }

        template<typename... Args>
        void Emplace(Args&&... args)
        {
            LockGuard lock(_mutex);

            _queue.emplace(std::forward<Args>(args)...);
        }

        void Clear()
        {
            LockGuard lock(_mutex);

            _queue = std::queue<TItem>();
        }

        LockBuffer& operator<<(LockBuffer& other)
        {
            std::lock(_mutex, other._mutex);
            UniqueLock selfLock(_mutex, std::adopt_lock);
            UniqueLock otherLock(other._mutex, std::adopt_lock);

            if (_queue.empty())
            {
                _queue = std::move(other._queue);

                return *this;
            }

            while (!other._queue.empty())
            {
                _queue.push(std::move(other._queue.front()));
                other._queue.pop();
            }

            return *this;
        }

        LockBuffer& operator<<(std::queue<TItem>& other)
        {
            LockGuard lock(_mutex);

            if (_queue.empty())
            {
                _queue = std::move(other);

                return *this;
            }

            while (!other.empty())
            {
                _queue.push(std::move(other.front()));
                other.pop();
            }

            return *this;
        }

        LockBuffer& operator>>(LockBuffer& other)
        {
            std::lock(_mutex, other._mutex);
            UniqueLock selfLock(_mutex, std::adopt_lock);
            UniqueLock otherLock(other._mutex, std::adopt_lock);

            if (other._queue.empty())
            {
                other._queue = std::move(_queue);

                return *this;
            }

            while (!_queue.empty())
            {
                other._queue.push(std::move(_queue.front()));
                _queue.pop();
            }

            return *this;
        }

        LockBuffer& operator>>(std::queue<TItem>& other)
        {
            LockGuard lock(_mutex);

            if (other.empty())
            {
                other = std::move(_queue);

                return *this;
            }

            while (!_queue.empty())
            {
                other.push(std::move(_queue.front()));
                _queue.pop();
            }

            return *this;
        }

    private:
        std::queue<TItem>   _queue;
        Mutex               _mutex;

    };
}
