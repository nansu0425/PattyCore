#pragma once

namespace PattyCore
{
    /*------------------*
     *    LockBuffer    *
     *------------------*/

    template<typename TItem>
    class LockBuffer
    {
    public:
        LockBuffer() = default;
        LockBuffer(const LockBuffer&) = delete;
        LockBuffer(LockBuffer&&) = delete;
        LockBuffer& operator=(const LockBuffer&) = delete;
        LockBuffer& operator=(LockBuffer&&) = delete;

        void Push(TItem&& item)
        {
            MutexLockGrd lock(mMutex);

            mQueue.push(std::move(item));
        }

        bool Pop(TItem& item)
        {
            MutexLockGrd lock(mMutex);

            if (mQueue.empty())
            {
                return false;
            }

            item = std::move(mQueue.front());
            mQueue.pop();

            return true;
        }

        template<typename... Args>
        void Emplace(Args&&... args)
        {
            MutexLockGrd lock(mMutex);

            mQueue.emplace(std::forward<Args>(args)...);
        }

        void Clear()
        {
            MutexLockGrd lock(mMutex);

            mQueue = std::queue<TItem>();
        }

        LockBuffer& operator<<(LockBuffer& other)
        {
            std::lock(mMutex, other.mMutex);
            MutexULock selfLock(mMutex, std::adopt_lock);
            MutexULock otherLock(other.mMutex, std::adopt_lock);

            if (mQueue.empty())
            {
                mQueue = std::move(other.mQueue);

                return *this;
            }

            while (!other.mQueue.empty())
            {
                mQueue.push(std::move(other.mQueue.front()));
                other.mQueue.pop();
            }

            return *this;
        }

        LockBuffer& operator<<(std::queue<TItem>& other)
        {
            MutexLockGrd lock(mMutex);

            if (mQueue.empty())
            {
                mQueue = std::move(other);

                return *this;
            }

            while (!other.empty())
            {
                mQueue.push(std::move(other.front()));
                other.pop();
            }

            return *this;
        }

        LockBuffer& operator>>(LockBuffer& other)
        {
            std::lock(mMutex, other.mMutex);
            MutexULock selfLock(mMutex, std::adopt_lock);
            MutexULock otherLock(other.mMutex, std::adopt_lock);

            if (other.mQueue.empty())
            {
                other.mQueue = std::move(mQueue);

                return *this;
            }

            while (!mQueue.empty())
            {
                other.mQueue.push(std::move(mQueue.front()));
                mQueue.pop();
            }

            return *this;
        }

        LockBuffer& operator>>(std::queue<TItem>& other)
        {
            MutexLockGrd lock(mMutex);

            if (other.empty())
            {
                other = std::move(mQueue);

                return *this;
            }

            while (!mQueue.empty())
            {
                other.push(std::move(mQueue.front()));
                mQueue.pop();
            }

            return *this;
        }

    private:
        std::queue<TItem>   mQueue;
        Mutex               mMutex;

    };
}
