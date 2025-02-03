#pragma once

namespace Server
{
    /*---------------*
     *    Service    *
     *---------------*/

    class Service : public ServerServiceBase
    {
    public:
        Service(const Threads::Info& threadsInfo, uint16_t port);

    protected:
        virtual void OnMessageReceived(OwnedMessage ownedMessage) override;

    private:
        void HandlePing(Session::Pointer pSession);
        void WaitSecondAsync();
        void OnSecondElapsed(const ErrorCode& error);

    private:
        Timer                   _secondTimer;
        std::atomic<uint32_t>   _nMessagesHandled = 0;
    };
}
