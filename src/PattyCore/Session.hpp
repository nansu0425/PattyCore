#pragma once

#include <PattyCore/Include.hpp>
#include <PattyCore/Message.hpp>

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
        using OwnedMessage      = OwnedMessage<Session>;
        using OnClosed          = std::function<void(const ErrorCode&, Pointer)>;
        using OnReceived        = std::function<void(OwnedMessage&&)>;

        ~Session()
        {
            std::cout << *this << " Session destroyed: " << GetEndpoint() << "\n";
        }

        static Pointer Create(Tcp::socket&& socket,
                              const Id id,
                              OnClosed onClosed,
                              Strand&& writeStrand,
                              OnReceived onReceived)
        {
            Pointer pSelf = Pointer(new Session(std::move(socket),
                                                id,
                                                std::move(onClosed),
                                                std::move(writeStrand),
                                                std::move(onReceived)));
            pSelf->ReceiveAsync(pSelf);

            return pSelf;
        }

        void SendAsync(Message&& message)
        {
            Message::Pointer pMessage = std::make_unique<Message>(std::move(message));

            asio::post(_writeStrand,
                       [pSelf = shared_from_this(), pMessage = std::move(pMessage)]() mutable
                       {
                           pSelf->WriteHeaderAsync(std::move(pMessage));
                       });
        }
        
        void Close()
        {
            ErrorCode error;

            {
                UniqueSharedLock lock(_sharedMutex);

                if (!_socket.is_open())
                {
                    return;
                }
                
                _socket.close(error);
            }

            _onClosed(error, shared_from_this());
        }

        Id GetId() const noexcept
        {
            return _id;
        }

        const Tcp::endpoint& GetEndpoint() const noexcept
        {
            return _endpoint;
        }
        
        friend std::ostream& operator<<(std::ostream& os, const Session& session)
        {
            os << "[" << session.GetId() << "]";

            return os;
        }

    private:
        Session(Tcp::socket&& socket,
                const Id id,
                OnClosed&& onClosed,
                Strand&& writeStrand,
                OnReceived&& onReceived)
            : _socket(std::move(socket))
            , _id(id)
            , _endpoint(_socket.remote_endpoint())
            , _onClosed(std::move(onClosed))
            , _writeStrand(std::move(writeStrand))
            , _onReceived(std::move(onReceived))
        {
            std::cout << *this << " Session created: " << GetEndpoint() << "\n";
        }

        void WriteHeaderAsync(Message::Pointer pMessage)
        {
            SharedLock lock(_sharedMutex);
            Message::Header& header = pMessage->header;

            asio::async_write(_socket,
                              asio::buffer(&header, sizeof(Message::Header)),
                              asio::bind_executor(_writeStrand,
                                                  [pSelf = shared_from_this(), pMessage = std::move(pMessage)]
                                                  (const ErrorCode& error, const size_t nBytesTransferred) mutable
                                                  {
                                                      pSelf->OnHeaderWritten(error, nBytesTransferred, std::move(pMessage));
                                                  }));
        }

        void OnHeaderWritten(const ErrorCode& error, const size_t nBytesTransferred, Message::Pointer pMessage)
        {
            if (error)
            {
                std::cerr << *this << " Failed to write header : " << error << "\n";
            }
            else
            {
                assert(sizeof(Message::Header) == nBytesTransferred);

                // The size of payload is bigger than 0
                if (pMessage->header.size > sizeof(Message::Header))
                {
                    WritePayloadAsync(std::move(pMessage));

                    return;
                }
            }

            OnMessageWritten(error);
        }

        void WritePayloadAsync(Message::Pointer pMessage)
        {
            SharedLock lock(_sharedMutex);
            Message::Payload& payload = pMessage->payload;

            asio::async_write(_socket,
                              asio::buffer(payload),
                              [pSelf = shared_from_this(), pMessage = std::move(pMessage)]
                              (const ErrorCode& error, const size_t nBytesTransferred) mutable
                              {
                                  pSelf->OnPayloadWritten(error, nBytesTransferred, std::move(pMessage));
                              });
        }

        void OnPayloadWritten(const ErrorCode& error, const size_t nBytesTransferred, Message::Pointer pMessage)
        {
            if (error)
            {
                std::cerr << *this << " Failed to write payload: " << error << "\n";
            }
            else
            {
                assert(nBytesTransferred == pMessage->payload.size());
            }

            OnMessageWritten(error);
        }

        void OnMessageWritten(const ErrorCode& error)
        {
            if (error)
            {
                Close();
            }
        }

        void ReceiveAsync(Pointer pSelf)
        {
            assert(_readMessage.pOwner == nullptr);
            _readMessage.pOwner = std::move(pSelf);

            ReadHeaderAsync();
        }

        void ReadHeaderAsync()
        {
            SharedLock lock(_sharedMutex);

            asio::async_read(_socket,
                             asio::buffer(&_readMessage.message.header, sizeof(Message::Header)),
                             [this](const ErrorCode& error, const size_t nBytesTransferred)
                             {
                                 OnHeaderRead(error, nBytesTransferred);
                             });
        }

        void OnHeaderRead(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (error)
            {
                std::cerr << *this << " Failed to read header: " << error << "\n";
            }
            else
            {
                assert(nBytesTransferred == sizeof(Message::Header));
                assert(_readMessage.message.header.size >= sizeof(Message::Header));

                // The size of payload is bigger than 0
                if (_readMessage.message.header.size > sizeof(Message::Header))
                {
                    _readMessage.message.payload.resize(_readMessage.message.header.size - sizeof(Message::Header));
                    ReadPayloadAsync();

                    return;
                }
            }

            OnMessageRead(error);
        }

        void ReadPayloadAsync()
        {
            SharedLock lock(_sharedMutex);

            asio::async_read(_socket,
                             asio::buffer(_readMessage.message.payload),
                             [this](const ErrorCode& error, const size_t nBytesTransferred)
                             {
                                 OnPayloadRead(error, nBytesTransferred);
                             });
        }

        void OnPayloadRead(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (error)
            {
                std::cerr << *this << " Failed to read payload: " << error << "\n";
            }
            else
            {
                assert(_readMessage.message.payload.size() == nBytesTransferred);
            }

            OnMessageRead(error);
        }

        void OnMessageRead(const ErrorCode& error)
        {
            Pointer pSelf = std::move(_readMessage.pOwner);

            if (error)
            {
                Close();

                return;
            }

            _readMessage.pOwner = pSelf;
            _onReceived(std::move(_readMessage));

            ReceiveAsync(std::move(pSelf));
        }

    private:
        Tcp::socket             _socket;
        SharedMutex             _sharedMutex;
        const Id                _id;
        const Tcp::endpoint     _endpoint;
        OnClosed                _onClosed;

        Strand                  _writeStrand;

        OwnedMessage            _readMessage;
        OnReceived              _onReceived;

    };
}
