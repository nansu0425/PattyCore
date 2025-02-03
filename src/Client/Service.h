#pragma once

namespace Client
{
    class Service : public ClientServiceBase
    {
    public:
        Service(const Threads::Info& threadsInfo);

    protected:
        virtual void OnSessionRegistered(Session::Pointer pSession) override;
        virtual void OnSessionUnregistered(Session::Pointer pSession) override;
        virtual void OnMessageReceived(OwnedMessage ownedMessage) override;

    private:
        void Ping(Session::Pointer pSession);
        void HandlePing(Session::Pointer pSession);
        void WaitPingTimerAsync(Session::Pointer pSession);
        void OnPingTimerExpired(const ErrorCode& error, Session::Pointer pSession);
        void PingAsync(Session::Pointer pSession);

    private:
        struct PingTimer
        {
            using Pointer = std::unique_ptr<PingTimer>;
            using Map = std::unordered_map<Session::Id, Pointer>;

            Session::Id     id;
            Timer           timer;
            TimePoint       start;

            PingTimer(Session::Id id, ThreadPool& timerWorkers);
            ~PingTimer();
        };

        PingTimer::Map      _pingTimerMap;
        Strand              _pingTimerStrand;
    };
}
