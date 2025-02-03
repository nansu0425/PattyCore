#pragma once

#include "ServiceBase.h"

namespace PattyCore
{
    /*-------------------------*
     *    ClientServiceBase    *
     *-------------------------*/

    class ClientServiceBase : public ServiceBase
    {
    public:
        ClientServiceBase(const ThreadPoolGroup::Info& threadsInfo);

        void Start(const std::string& host, const std::string& service, size_t numConnects);

    private:
        void ConnectAsync(size_t numConnects);
        void OnConnected(const ErrCode& error, Tcp::socket socket, size_t numConnects);

    private:
        Tcp::resolver       mResolver;
        Endpoints           mEndpoints;
        Tcp::socket         mSocket;
    };
}
