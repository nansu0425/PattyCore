#pragma once

namespace Client
{
    /*---------------*
     *    Service    *
     *---------------*/

    class Service : public ClientServiceBase
    {
    public:
        Service(const ThreadPoolGroup::Info& info);

    protected:
        virtual void OnSessionRegistered(Session::Ptr session) override;
        virtual void OnSessionUnregistered(Session::Ptr session) override;
        virtual void OnMessageReceived(OwnedMessage ownedMessage) override;

    private:
        void Ping(Session::Ptr session);
        void HandlePing(Session::Ptr session);
        void WaitPingTimerAsync(Session::Ptr session);
        void OnPingTimerExpired(const ErrCode& errCode, Session::Ptr session);
        void PingAsync(Session::Ptr session);

    private:
        /*-----------------*
         *    PingTimer    *
         *-----------------*/

        struct PingTimer
        {
            using Ptr           = UPtr<PingTimer>;
            using Map           = std::unordered_map<Session::Id, Ptr>;

            const Session::Id   id;
            Timer               timer;
            TimePoint           start;

            PingTimer(const Session::Id id, ThreadPool& taskGroup);
            ~PingTimer();
        };

    private:
        PingTimer::Map      mPingTimerMap;
        Strand              mPingTimerStrand;

    };
}
