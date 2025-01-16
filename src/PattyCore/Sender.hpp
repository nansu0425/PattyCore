#pragma once

#include <PattyCore/Message.hpp>
#include <PattyCore/SessionManager.hpp>

namespace PattyCore
{
    /*--------------*
     *    Sender    *
     *--------------*/

    class Sender
        : public std::enable_shared_from_this<Sender>
    {
    public:
        using Pointer           = std::shared_ptr<Sender>;

    public:
        ~Sender()
        {
            std::cout << "[" << _sessionId << "] Sender destroyed\n";
        }

        static Pointer Create(ThreadPool& workers,
                              SessionManager& sessionManager,
                              Session::Id sessionId)
        {
            return Pointer(new Sender(workers,
                                      sessionManager,
                                      sessionId));
        }

        template<typename TMessage, typename TCallback>
        void SendMessageAsync(TMessage&& message, TCallback&& callback)
        {
            asio::post(_strand,
                       [pSelf = shared_from_this(), 
                        message = std::forward<TMessage>(message),
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           pSelf->PushMessage(std::move(message), 
                                              std::move(callback));
                       });
        }

        const Session::Id GetSessionId() const
        {
            return _sessionId;
        }

        friend std::ostream& operator<<(std::ostream& os, Pointer pSender)
        {
            os << "[" << pSender->GetSessionId() << "]";

            return os;
        }

    private:
        Sender(ThreadPool& workers,
               SessionManager& sessionManager,
               Session::Id sessionId)
            : _sessionManager(sessionManager)
            , _sessionId(sessionId)
            , _strand(asio::make_strand(workers))
            , _isWriting(false)
        {}

        template<typename TMessage, typename TCallback>
        void PushMessage(TMessage&& message, TCallback&& callback)
        {
            _buffer.emplace(std::forward<TMessage>(message));

            WriteMessageAsync(std::forward<TCallback>(callback));
        }

        template<typename TCallback>
        void WriteMessageAsync(TCallback&& callback)
        {
            if (_isWriting ||
                _buffer.empty())
            {
                return;
            }

            _message = std::move(_buffer.front());
            _buffer.pop();

            WriteHeaderAsync(std::forward<TCallback>(callback));

            _isWriting = true;
        }

        template<typename TCallback>
        void WriteHeaderAsync(TCallback&& callback)
        {
            _sessionManager.WriteAsync(_sessionId,
                                      asio::buffer(&_message.header,
                                                   sizeof(Message::Header)),
                                      [pSelf = shared_from_this(),
                                       callback = std::forward<TCallback>(callback)]
                                      (const ErrorCode& error,
                                       const size_t nBytesTransferred) mutable
                                      {
                                          pSelf->OnHeaderWritten(error,
                                                                 nBytesTransferred,
                                                                 std::move(callback));
                                      });
        }

        template<typename TCallback>
        void OnHeaderWritten(const ErrorCode& error, const size_t nBytesTransferred, TCallback&& callback)
        {
            if (error)
            {
                std::cerr << "[" << _sessionId << "] Failed to write header: " << error << "\n";
            }
            else
            {
                assert(sizeof(Message::Header) == nBytesTransferred);

                // The size of payload is bigger than 0
                if (_message.header.size > sizeof(Message::Header))
                {
                    WritePayloadAsync(std::forward<TCallback>(callback));

                    return;
                }
            }

            asio::post(_strand,
                       [pSelf = shared_from_this(), 
                        error,
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           pSelf->OnMessageWritten(error,
                                                   std::move(callback));
                       });
        }

        template<typename TCallback>
        void WritePayloadAsync(TCallback&& callback)
        {
            _sessionManager.WriteAsync(_sessionId,
                                      asio::buffer(_message.payload.data(),
                                                   _message.payload.size()),
                                      [pSelf = shared_from_this(),
                                       callback = std::forward<TCallback>(callback)]
                                      (const ErrorCode& error, 
                                       const size_t nBytesTransferred) mutable
                                      {
                                          pSelf->OnPayloadWritten(error, 
                                                                  nBytesTransferred,
                                                                  std::move(callback));
                                      });
        }

        template<typename TCallback>
        void OnPayloadWritten(const ErrorCode& error, const size_t nBytesTransferred, TCallback&& callback)
        {
            if (error)
            {
                std::cerr << "[" << _sessionId << "] Failed to write payload: " << error << "\n";
            }
            else
            {
                assert(nBytesTransferred == _message.payload.size());
            }

            asio::post(_strand,
                       [pSelf = shared_from_this(), 
                        error,
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           pSelf->OnMessageWritten(error,
                                                   std::move(callback));
                       });
        }

        template<typename TCallback>
        void OnMessageWritten(const ErrorCode& error, TCallback&& callback)
        {
            _isWriting = false;

            if (callback(error, _sessionId, _message))
            {
                WriteMessageAsync(std::forward<TCallback>(callback));   
            }
        }

    private:
        SessionManager&             _sessionManager;
        const Session::Id           _sessionId;
        Message::Buffer             _buffer;
        Strand                      _strand;
        Message                     _message;
        bool                        _isWriting;

    };
}
