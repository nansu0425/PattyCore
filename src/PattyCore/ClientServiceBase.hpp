#pragma once

#include <PattyCore/ServiceBase.hpp>

namespace PattyCore
{
    /*-------------------------*
     *    ClientServiceBase    *
     *-------------------------*/

    class ClientServiceBase : public ServiceBase
    {
    protected:
        using SocketBuffer      = std::queue<Tcp::socket>;

    public:
        ClientServiceBase(size_t nIo,
                          size_t nControl,
                          size_t nHandler,
                          size_t nTimer)
            : ServiceBase(nIo,
                          nControl,
                          nHandler,
                          nTimer)
            , _resolver(_workers.control)
            , _socket(_workers.io)
        {}

        void Start(const std::string& host, const std::string& service, size_t nConnects)
        {
            ErrorCode error;

            _endpoints = _resolver.resolve(host, service, error);

            if (error)
            {
                std::cerr << "[CLIENT] Failed to resolve: " << error << "\n";
                return;
            }

            ConnectAsync(nConnects);
            std::cout << "[CLIENT] Started!\n";
        }

    private:
        void ConnectAsync(size_t nConnects)
        {
            if (nConnects == 0)
            {
                return;
            }

            asio::async_connect(_socket,
                                _endpoints,
                                asio::bind_executor(_workers.control,
                                                    [this, nConnects]
                                                    (const ErrorCode& error,
                                                     const Tcp::endpoint& endpoint)
                                                    {
                                                        OnConnected(error,
                                                                    std::move(_socket),
                                                                    nConnects);
                                                    })
                                );
        }

        void OnConnected(const ErrorCode& error, Tcp::socket socket, size_t nConnects)
        {
            if (error)
            {
                std::cerr << "[CLIENT] Failed to connect: " << error << "\n";
                return;
            }

            _socket = Tcp::socket(_workers.io);
            ConnectAsync(--nConnects);

            CreateSession(std::move(socket));
        }

    private:
        Tcp::resolver       _resolver;
        Endpoints           _endpoints;
        Tcp::socket         _socket;

    };
}
