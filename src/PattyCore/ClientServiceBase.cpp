#include "Pch.h"
#include "ClientServiceBase.h"

namespace PattyCore
{
    ClientServiceBase::ClientServiceBase(const ThreadPoolGroup::Info& threadsInfo)
        : ServiceBase(threadsInfo)
        , mResolver(mThreadPoolGroup.GetSessionGroup())
        , mSocket(mThreadPoolGroup.GetSocketGroup())
    {}

    void ClientServiceBase::Start(const std::string& host, const std::string& service, size_t numConnects)
    {
        ErrCode errCode;

        mEndpoints = mResolver.resolve(host, service, errCode);

        if (errCode)
        {
            std::cerr << "[CLIENT] Failed to resolve: " << errCode << "\n";
            return;
        }

        ConnectAsync(numConnects);
        std::cout << "[CLIENT] Started!\n";
    }

    void ClientServiceBase::ConnectAsync(size_t numConnects)
    {
        if (numConnects == 0)
        {
            return;
        }

        asio::async_connect(mSocket, 
                            mEndpoints,
                            asio::bind_executor(mThreadPoolGroup.GetSessionGroup(),
                                                [this, numConnects]
                                                (const ErrCode& errCode, const Tcp::endpoint& endpoint)
                                                {
                                                    OnConnected(errCode, std::move(mSocket), numConnects);
                                                })
        );
    }

    void ClientServiceBase::OnConnected(const ErrCode& errCode, Tcp::socket socket, size_t numConnects)
    {
        if (errCode)
        {
            std::cerr << "[CLIENT] Failed to connect: " << errCode << "\n";
            return;
        }

        mSocket = Tcp::socket(mThreadPoolGroup.GetSocketGroup());
        ConnectAsync(--numConnects);

        CreateSession(std::move(socket));
    }
}
