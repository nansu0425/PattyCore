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

        ~Session()
        {
            std::cout << *this << " Session destroyed: " << GetEndpoint() << "\n";
        }

        static Pointer Create(ThreadPool& workers,
                              Tcp::socket&& socket,
                              const Id id,
                              OnClosed onClosed)
        {
            Pointer pSelf = Pointer(new Session(workers,
                                                std::move(socket),
                                                id,
                                                std::move(onClosed)));
            pSelf->ReadMessageAsync(pSelf);

            return pSelf;
        }

        void Dispatch(OwnedMessage&& ownedMessage)
        {
            assert(ownedMessage.pOwner.get() == this);
            _writeBuffer.Push(std::move(ownedMessage.message));

            asio::post(_writeStrand,
                       [pSelf = std::move(ownedMessage.pOwner)]() mutable
                       {
                           pSelf->WriteMessageAsync(std::move(pSelf));
                       });
        }

        void Fetch(OwnedMessage::Buffer& buffer)
        {
            buffer << _readBuffer;
        }
        
        void Close(Pointer pSelf)
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

            _onClosed(error, std::move(pSelf));
        }

        Id GetId() const noexcept
        {
            return _id;
        }

        const Tcp::endpoint& GetEndpoint() const noexcept
        {
            return _endpoint;
        }

        void SetOnClosed(OnClosed onClosed)
        {
            _onClosed = std::move(onClosed);
        }
        
        friend std::ostream& operator<<(std::ostream& os, const Session& session)
        {
            os << "[" << session.GetId() << "]";

            return os;
        }

    private:
        Session(ThreadPool& workers,
                Tcp::socket&& socket,
                const Id id,
                OnClosed&& onClosed)
            : _socket(std::move(socket))
            , _id(id)
            , _endpoint(_socket.remote_endpoint())
            , _onClosed(std::move(onClosed))
            , _writeStrand(asio::make_strand(workers))
            , _isWriting(false)
        {
            std::cout << *this << " Session created: " << GetEndpoint() << "\n";
        }

        void WriteMessageAsync(Pointer pSelf)
        {
            if (_isWriting)
            {
                return;
            }

            if (!_writeBuffer.Pop(_writeMessage.message))
            {
                return;
            }

            assert(_writeMessage.pOwner == nullptr);

            _writeMessage.pOwner = std::move(pSelf);
            WriteHeaderAsync();

            _isWriting = true;
        }

        void WriteHeaderAsync()
        {
            SharedLock lock(_sharedMutex);

            asio::async_write(_socket,
                              asio::buffer(&_writeMessage.message.header,
                                           sizeof(Message::Header)),
                              [this]
                              (const ErrorCode& error,
                               const size_t nBytesTransferred)
                              {
                                  OnHeaderWritten(error,
                                                  nBytesTransferred);
                              });
        }

        void OnHeaderWritten(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (error)
            {
                std::cerr << *this << " Failed to write header : " << error << "\n";
            }
            else
            {
                assert(sizeof(Message::Header) == nBytesTransferred);

                // The size of payload is bigger than 0
                if (_writeMessage.message.header.size > sizeof(Message::Header))
                {
                    WritePayloadAsync();

                    return;
                }
            }

            OnMessageWritten(error);
        }

        void WritePayloadAsync() 
        {
            SharedLock lock(_sharedMutex);

            asio::async_write(_socket,
                              asio::buffer(_writeMessage.message.payload),
                              [this]
                              (const ErrorCode& error,
                               const size_t nBytesTransferred)
                              {
                                  OnPayloadWritten(error,
                                                   nBytesTransferred);
                              });
        }

        void OnPayloadWritten(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (error)
            {
                std::cerr << *this << " Failed to write payload: " << error << "\n";
            }
            else
            {
                assert(nBytesTransferred == _writeMessage.message.payload.size());
            }

            OnMessageWritten(error);
        }

        void OnMessageWritten(const ErrorCode& error)
        {
            Pointer pSelf = std::move(_writeMessage.pOwner);
            assert(pSelf != nullptr);

            if (error)
            {
                Close(std::move(pSelf));

                return;
            }

            asio::post(_writeStrand,
                       [pSelf = std::move(pSelf)]() mutable
                       {
                           pSelf->_isWriting = false;
                           pSelf->WriteMessageAsync(std::move(pSelf));
                       });
        }

        void ReadMessageAsync(Pointer pSelf)
        {
            assert(pSelf.get() == this);
            assert(_readMessage.pOwner == nullptr);

            _readMessage.pOwner = std::move(pSelf);

            ReadHeaderAsync();
        }

        void ReadHeaderAsync()
        {
            SharedLock lock(_sharedMutex);

            asio::async_read(_socket,
                             asio::buffer(&_readMessage.message.header,
                                          sizeof(Message::Header)),
                             [this](const ErrorCode& error,
                                    const size_t nBytesTransferred)
                             {
                                 OnHeaderRead(error, 
                                              nBytesTransferred);
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
                             [this](const ErrorCode& error,
                                    const size_t nBytesTransferred)
                             {
                                 OnPayloadRead(error,
                                               nBytesTransferred);
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
                Close(std::move(pSelf));
                _readBuffer.Clear();

                return;
            }

            _readBuffer.Emplace(shared_from_this(),
                                std::move(_readMessage.message));

            ReadMessageAsync(std::move(pSelf));
        }

    private:
        Tcp::socket             _socket;
        SharedMutex             _sharedMutex;
        const Id                _id;
        const Tcp::endpoint     _endpoint;
        OnClosed                _onClosed;

        // Write
        Message::Buffer         _writeBuffer;
        OwnedMessage            _writeMessage;
        Strand                  _writeStrand;
        bool                    _isWriting;

        // Read
        OwnedMessage::Buffer    _readBuffer;
        OwnedMessage            _readMessage;

    };
}
