#include "Pch.h"
#include "ServerServiceBase.h"

namespace PattyCore
{
    ServerServiceBase::ServerServiceBase(const ThreadPoolGroup::Info& info, uint16_t port)
        : ServiceBase(info)
        , mAcceptor(mThreadPoolGroup.GetSessionGroup(), Tcp::endpoint(Tcp::v4(), port))
        , mSocket(mThreadPoolGroup.GetSocketGroup())
    {}

    void ServerServiceBase::Start()
    {
        AcceptAsync();
        std::cout << "[SERVER] Started!\n";
    }

    void ServerServiceBase::AcceptAsync()
    {
        mAcceptor.async_accept(mSocket,
                               [this](const ErrCode& errCode)
                               {
                                   OnAccepted(errCode, std::move(mSocket));
                               });
    }

    void ServerServiceBase::OnAccepted(const ErrCode& errCode, Tcp::socket socket)
    {
        if (errCode)
        {
            std::cerr << "[SERVER] Failed to accept: " << errCode << "\n";
            return;
        }

        mSocket = Tcp::socket(mThreadPoolGroup.GetSocketGroup());
        AcceptAsync();

        CreateSession(std::move(socket));
    }
}
