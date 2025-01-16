#pragma once

#include <PattyCore/Include.hpp>

namespace PattyCore
{
    /*---------------*
     *    Session    *
     *---------------*/

    class Session
        : public std::enable_shared_from_this<Session>
    {
    public:
        using Id                = uint32_t;
        using Pointer           = std::shared_ptr<Session>;

        ~Session()
        {
            std::cout << "[" << _id << "] Session destroyed: " << _endpoint << "\n";
        }

        static Pointer Create(ThreadPool& workers,
                              Tcp::socket&& socket,
                              Id id)
        {
            return Pointer(new Session(workers,
                                       std::move(socket),
                                       id));
        }

        template<typename TBuffer, typename TCallback>
        void WriteAsync(TBuffer&& buffer, TCallback&& callback)
        {
            asio::post(_strand,
                       [pSelf = shared_from_this(),
                        buffer = std::forward<TBuffer>(buffer),
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           asio::async_write(pSelf->_socket,
                                             std::move(buffer),
                                             std::move(callback));
                       });
        }

        template<typename TBuffer, typename TCallback>
        void ReadAsync(TBuffer&& buffer, TCallback&& callback)
        {
            asio::post(_strand,
                       [pSelf = shared_from_this(),
                        buffer = std::forward<TBuffer>(buffer),
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           asio::async_read(pSelf->_socket,
                                            std::move(buffer),
                                            std::move(callback));
                       });
        }

        template<typename TCallback>
        void CloseAsync(TCallback&& callback)
        {
            asio::post(_strand,
                       [pSelf = shared_from_this(),
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           pSelf->Close(std::move(callback));
                       });
        }

        Id GetId() const
        {
            return _id;
        }

        const Tcp::endpoint& GetEndpoint() const
        {
            return _endpoint;
        }

    private:
        Session(ThreadPool& workers,
                Tcp::socket&& socket,
                Id id)
            : _socket(std::move(socket))
            , _strand(asio::make_strand(workers))
            , _id(id)
            , _endpoint(_socket.remote_endpoint())
        {}

        template<typename TCallback>
        void Close(TCallback&& callback)
        {
            if (!_socket.is_open())
            {
                return;
            }

            ErrorCode error;
            _socket.close(error);

            callback(error, _id);
        }

    private:
        Tcp::socket             _socket;
        Strand                  _strand;
        const Id                _id;
        const Tcp::endpoint     _endpoint;

    };
}
