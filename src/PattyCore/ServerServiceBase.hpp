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
            , _acceptor(_workers.control, Tcp::endpoint(Tcp::v4(), port))
            , _socket(_workers.io)
        {}

        void Start()
        {
            AcceptAsync();
            std::cout << "[SERVER] Started!\n";
        }

    private:
        void AcceptAsync()
        {
            _acceptor.async_accept(_socket,
                                   [this](const ErrorCode& error)
                                   {
                                       OnAccepted(error, std::move(_socket));
                                   });
        }

        void OnAccepted(const ErrorCode& error, Tcp::socket socket)
        {
            if (error)
            {
                std::cerr << "[SERVER] Failed to accept: " << error << "\n";
                return;
            }

            _socket = Tcp::socket(_workers.io);
            AcceptAsync();

            CreateSession(std::move(socket));
        }

    protected:
        Tcp::acceptor       _acceptor;
        Tcp::socket         _socket;

    };
}
