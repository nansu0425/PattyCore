#pragma once

namespace Server
{
    /*---------------*
     *    Service    *
     *---------------*/

    class Service : public ServerServiceBase
    {
    public:
        Service(const ThreadPoolGroup::Info& info, uint16_t port);

    protected:
        virtual void OnMessageReceived(OwnedMessage ownedMsg) override;

    private:
        void HandlePing(Session::Ptr session);
        void WaitSecondAsync();
        void OnSecondElapsed(const ErrCode& errCode);

    private:
        Timer                   mSecondTimer;
        std::atomic<uint32_t>   mNumMsgsHandled = 0;
    };
}
