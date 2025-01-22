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
        ServerServiceBase(size_t nIoHandlers,
                          size_t nControllers,
                          size_t nMessageHandlers,
                          size_t nTimers,
                          uint16_t port)
            : ServiceBase(nIoHandlers,
                          nControllers,
                          nMessageHandlers,
                          nTimers)
            , _acceptor(_workers.controllers, Tcp::endpoint(Tcp::v4(), port))
            , _socket(_workers.ioHandlers)
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

            _socket = Tcp::socket(_workers.ioHandlers);
            AcceptAsync();

            CreateSession(std::move(socket));
        }

    protected:
        Tcp::acceptor       _acceptor;
        Tcp::socket         _socket;

    };
}
