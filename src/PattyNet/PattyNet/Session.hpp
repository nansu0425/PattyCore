#pragma once

#include <PattyNet/Include.hpp>
#include <PattyNet/Message.hpp>

namespace PattyNet
{
    class Session 
        : public std::enable_shared_from_this<Session>
    {
    public:
        using Pointer               = std::shared_ptr<Session>;
        using Id                    = uint32_t;
        using OwnedMessage          = OwnedMessage<Session>;
        using OwnedMessageBuffer    = OwnedMessage::Buffer;
        using ThreadPool            = asio::thread_pool;
        using Strand                = asio::strand<ThreadPool::executor_type>;
        using ErrorCode             = asio::error_code;
        using Tcp                   = asio::ip::tcp;
        using Endpoints             = asio::ip::basic_resolver_results<Tcp>;
        using CloseCallback         = std::function<void(Pointer)>;
        using MessageBuffer         = Message::Buffer;

    public:
        ~Session()
        {
            std::cout << "[" << _id << "] Session destroyed: " << _endpoint << "\n";
        }

        static Pointer Create(ThreadPool& workers,
                              Tcp::socket&& socket,
                              Id id,
                              CloseCallback onSessionClosed,
                              OwnedMessageBuffer& receiveBuffer,
                              Strand& receiveStrand)
        {
            return Pointer(new Session(workers,
                                       std::move(socket),
                                       id,
                                       std::move(onSessionClosed),
                                       receiveBuffer,
                                       receiveStrand));
        }

        void CloseAsync()
        {
            asio::post(_socketStrand,
                       [pSelf = shared_from_this()]()
                       {
                           pSelf->Close();
                       });
        }

        template<typename TMessage>
        void SendMessageAsync(TMessage&& message)
        {
            asio::post(_sendStrand,
                       [pSelf = shared_from_this(), 
                       message = std::forward<TMessage>(message)]() mutable
                       {
                           pSelf->PushMessageToSendBuffer(std::move(message));
                       });
        }

        void ReceiveMessageAsync()
        {
            ReadMessageAsync();
        }

        Id GetId() const
        {
            return _id;
        }

        const Tcp::endpoint& GetEndpoint() const
        {
            return _endpoint;
        }

        friend std::ostream& operator<<(std::ostream& os, Pointer pSession)
        {
            os << "[" << pSession->GetId() << "]";

            return os;
        }

    private:
        Session(ThreadPool& workers,
                Tcp::socket&& socket,
                Id id,
                CloseCallback&& onSessionClosed,
                OwnedMessageBuffer& receiveBuffer,
                Strand& receiveStrand)
            : _workers(workers)
            , _socket(std::move(socket))
            , _socketStrand(asio::make_strand(workers))
            , _id(id)
            , _endpoint(_socket.remote_endpoint())
            , _onSessionClosed(std::move(onSessionClosed))
            , _receiveBuffer(receiveBuffer)
            , _receiveStrand(receiveStrand)
            , _sendStrand(asio::make_strand(workers))
            , _isWritingMessage(false)
        {}

        void Close()
        {
            if (_socket.is_open())
            {
                _socket.close();
                _onSessionClosed(shared_from_this());
            }
        }

        template<typename TMessage>
        void PushMessageToSendBuffer(TMessage&& message)
        {
            _sendBuffer.emplace(std::forward<TMessage>(message));

            WriteMessageAsync();
        }

        void WriteMessageAsync()
        {
            if (_isWritingMessage ||
                _sendBuffer.empty())
            {
                return;
            }

            _writeMessage = std::move(_sendBuffer.front());
            _sendBuffer.pop();

            asio::post(_socketStrand,
                       [pSelf = shared_from_this()]()
                       {
                           pSelf->WriteHeaderAsync();
                       });

            _isWritingMessage = true;
        }

        void WriteHeaderAsync()
        {
            asio::async_write(_socket,
                              asio::buffer(&_writeMessage.header,
                                           sizeof(Message::Header)),
                              [pSelf = shared_from_this()](const ErrorCode& error,
                                                           const size_t nBytesTransferred)
                              {
                                  pSelf->OnWriteHeaderCompleted(error, nBytesTransferred);
                              });
        }

        void OnWriteHeaderCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (error)
            {
                std::cerr << "[" << _id << "] Failed to write header: " << error << "\n";
            }
            else
            {
                assert(sizeof(Message::Header) == nBytesTransferred);

                // The size of payload is bigger than 0
                if (_writeMessage.header.size > sizeof(Message::Header))
                {
                    asio::post(_socketStrand,
                               [pSelf = shared_from_this()]()
                               {
                                   pSelf->WritePayloadAsync();
                               });

                    return;
                }
            }

            asio::post(_sendStrand,
                       [pSelf = shared_from_this(), error]
                       {
                           pSelf->OnWriteMessageCompleted(error);
                       });
        }

        void WritePayloadAsync()
        {
            asio::async_write(_socket,
                              asio::buffer(_writeMessage.payload.data(),
                                           _writeMessage.payload.size()),
                              [pSelf = shared_from_this()](const ErrorCode& error,
                                                           const size_t nBytesTransferred)
                              {
                                  pSelf->OnWritePayloadCompleted(error, nBytesTransferred);
                              });
        }

        void OnWritePayloadCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (error)
            {
                std::cerr << "[" << _id << "] Failed to write payload: " << error << "\n";
            }
            else
            {
                assert(nBytesTransferred == _writeMessage.payload.size());
            }

            asio::post(_sendStrand,
                       [pSelf = shared_from_this(), error]
                       {
                           pSelf->OnWriteMessageCompleted(error);
                       });
        }

        void OnWriteMessageCompleted(const ErrorCode& error)
        {
            _isWritingMessage = false;

            if (error)
            {
                CloseAsync();
                return;
            }

            WriteMessageAsync();
        }

        void ReadMessageAsync()
        {
            asio::post(_socketStrand,
                       [pSelf = shared_from_this()]()
                       {
                           pSelf->ReadHeaderAsync();
                       });
        }

        void ReadHeaderAsync()
        {
            asio::async_read(_socket,
                             asio::buffer(&_readMessage.header,
                                          sizeof(Message::Header)),
                             [pSelf = shared_from_this()](const ErrorCode& error,
                                                          const size_t nBytesTransferred)
                             {
                                 pSelf->OnReadHeaderCompleted(error, nBytesTransferred);
                             });
        }

        void OnReadHeaderCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (error)
            {
                std::cerr << "[" << _id << "] Failed to read header: " << error << "\n";
            }
            else
            {
                assert(nBytesTransferred == sizeof(Message::Header));
                assert(_readMessage.header.size >= sizeof(Message::Header));

                // The size of payload is bigger than 0
                if (_readMessage.header.size > sizeof(Message::Header))
                {
                    _readMessage.payload.resize(_readMessage.header.size - sizeof(Message::Header));

                    asio::post(_socketStrand,
                               [pSelf = shared_from_this()]()
                               {
                                   pSelf->ReadPayloadAsync();
                               });

                    return;
                }
            }

            OnReadMessageCompleted(error);
        }

        void ReadPayloadAsync()
        {
            asio::async_read(_socket,
                             asio::buffer(_readMessage.payload.data(),
                                          _readMessage.payload.size()),
                             [pSelf = shared_from_this()](const ErrorCode& error,
                                                          const size_t nBytesTransferred)
                             {
                                 pSelf->OnReadPayloadCompleted(error, nBytesTransferred);
                             });
        }

        void OnReadPayloadCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (error)
            {
                std::cerr << "[" << _id << "] Failed to read payload: " << error << "\n";
            }
            else
            {
                assert(_readMessage.payload.size() == nBytesTransferred);
            }

            OnReadMessageCompleted(error);
        }

        void OnReadMessageCompleted(const ErrorCode& error)
        {
            if (error)
            {
                CloseAsync();
                return;
            }

            asio::post(_receiveStrand,
                       [pSelf = shared_from_this()]
                       {
                           pSelf->PushMessageToReceiveBuffer();
                       });
        }

        void PushMessageToReceiveBuffer()
        {
            _receiveBuffer.push(OwnedMessage{shared_from_this(), std::move(_readMessage)});

            ReadMessageAsync();
        }

    private:
        ThreadPool&                     _workers;
        Tcp::socket                     _socket;
        Strand                          _socketStrand;
        const Id                        _id;
        const Tcp::endpoint             _endpoint;

        // Unregister-Destroy
        CloseCallback                   _onSessionClosed;

        // Receive
        OwnedMessageBuffer&             _receiveBuffer;
        Strand&                         _receiveStrand;
        Message                         _readMessage;

        // Send
        MessageBuffer                   _sendBuffer;
        Strand                          _sendStrand;
        Message                         _writeMessage;
        bool                            _isWritingMessage;

    };
}
