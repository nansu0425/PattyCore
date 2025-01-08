﻿#pragma once

#include <PattyNet/ServiceBase.hpp>

namespace PattyNet
{
    class ClientServiceBase : public ServiceBase
    {
    protected:
        using Endpoints         = Session::Endpoints;
        using SocketBuffer      = std::queue<Tcp::socket>;

    public:
        ClientServiceBase(size_t nWorkers, 
                          size_t nMaxReceivedMessages,
                          uint16_t nConnects)
            : ServiceBase(nWorkers, nMaxReceivedMessages)
            , _connectStrand(asio::make_strand(_workers))
            , _resolver(_workers)
        {
            InitConnectBuffer(nConnects);
        }

        void Start(const std::string& host, const std::string& service)
        {   
            ResolveAsync(host, service);
            std::cout << "[CLIENT] Started!\n";
        }

    private:
        void InitConnectBuffer(const uint16_t nConnects)
        {
            assert(nConnects > 0);

            for (int i = 0; i < nConnects; ++i)
            {
                _connectBuffer.emplace(_workers);
            }
        }

        void ResolveAsync(const std::string& host, const std::string& service)
        {
            _resolver.async_resolve(host,
                                    service,
                                    [this](const ErrorCode& error,
                                           Endpoints endpoints)
                                    {
                                        OnResolveCompleted(error, std::move(endpoints));
                                    });
        }

        void OnResolveCompleted(const ErrorCode& error, Endpoints&& endpoints)
        {
            if (error)
            {
                std::cerr << "[CLIENT] Failed to resolve: " << error << "\n";
                return;
            }

            _endpoints = std::move(endpoints);
            ConnectAsync();
        }

        void ConnectAsync()
        {
            asio::async_connect(_connectBuffer.front(),
                                       _endpoints,
                                       [this](const ErrorCode& error,
                                              const Tcp::endpoint& endpoint)
                                       {
                                           OnConnectCompleted(error);
                                       });
        }

        void OnConnectCompleted(const ErrorCode& error)
        {
            if (error)
            {
                std::cerr << "[CLIENT] Failed to connect: " << error << "\n";
                return;
            }

            CreateSession(std::move(_connectBuffer.front()));
            _connectBuffer.pop();

            if (_connectBuffer.empty())
            {
                return;
            }

            ConnectAsync();
        }

    private:
        SocketBuffer        _connectBuffer;
        Strand              _connectStrand;
        Tcp::resolver       _resolver;
        Endpoints           _endpoints;

    };
}
