#pragma once

#include <PattyCore/ServiceBase.h>

namespace PattyCore
{
    /*-------------------------*
     *    ServerServiceBase    *
     *-------------------------*/

    class ServerServiceBase : public ServiceBase
    {
    public:
        ServerServiceBase(const Threads::Info& threadsInfo,
                          uint16_t port)
            : ServiceBase(threadsInfo)
            , _acceptor(_threads.SessionPool(), Tcp::endpoint(Tcp::v4(), port))
            , _socket(_threads.SocketPool())
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

            _socket = Tcp::socket(_threads.SocketPool());
            AcceptAsync();

            CreateSession(std::move(socket));
        }

    protected:
        Tcp::acceptor       _acceptor;
        Tcp::socket         _socket;

    };
}
