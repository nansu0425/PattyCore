#pragma once

#include "ServiceBase.h"

namespace PattyCore
{
    /*-------------------------*
     *    ServerServiceBase    *
     *-------------------------*/

    class ServerServiceBase : public ServiceBase
    {
    public:
        ServerServiceBase(const ThreadPoolGroup::Info& info, uint16_t port);

        void Start();

    private:
        void AcceptAsync();
        void OnAccepted(const ErrCode& errCode, Tcp::socket socket);

    protected:
        Tcp::acceptor       mAcceptor;
        Tcp::socket         mSocket;

    };
}
