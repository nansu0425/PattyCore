#pragma once

#include <PattyCore/ServiceBase.hpp>

namespace PattyCore
{
    /*-------------------------*
     *    ServerServiceBase    *
     *-------------------------*/

    class ServerServiceBase : public ServiceBase
    {
    public:
        ServerServiceBase(size_t nIoPool,
                          size_t nControlPool,
                          size_t nHandlerPool,
                          size_t nTimerPool,
                          uint16_t port)
            : ServiceBase(nIoPool,
                          nControlPool,
                          nHandlerPool,
                          nTimerPool)
            , _acceptor(_ioPool, Tcp::endpoint(Tcp::v4(), port))
        {}

        void Start()
        {
            Run();
            AcceptAsync();
            std::cout << "[SERVER] Started!\n";
        }

    private:
        void AcceptAsync()
        {
            _acceptor.async_accept([this](const ErrorCode& error,
                                          Tcp::socket socket)
                                   {
                                       OnAccepted(error, std::move(socket));
                                   });
        }

        void OnAccepted(const ErrorCode& error, Tcp::socket&& socket)
        {
            if (error)
            {
                std::cerr << "[SERVER] Failed to accept: " << error << "\n";
                return;
            }

            AcceptAsync();
            CreateSessionAsync(std::move(socket));
        }

    protected:
        Tcp::acceptor       _acceptor;

    };
}
