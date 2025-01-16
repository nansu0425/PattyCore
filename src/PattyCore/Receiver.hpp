#pragma once

#include <PattyCore/Message.hpp>
#include <PattyCore/SessionManager.hpp>

namespace PattyCore
{
    /*----------------*
     *    Receiver    *
     *----------------*/

    class Receiver
    {
    public:
        using OwnedMessage      = OwnedMessage<Session::Id>;

    public:
        Receiver(ThreadPool& workers,
                 SessionManager& sessionManager)
            : _sessionManager(sessionManager)
            , _strand(asio::make_strand(workers))
        {}

        template<typename TCallback>
        void ReceiveMessageAsync(Session::Id sessionId, TCallback&& callback)
        {
            ReadHeaderAsync(sessionId,
                            std::forward<TCallback>(callback));
        }

        template<typename TCallback>
        void FetchMessagesAsync(OwnedMessage::Buffer& buffer, TCallback&& callback)
        {
            asio::post(_strand,
                       [this,
                        &buffer,
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           buffer = std::move(_buffer);
                           
                           callback();
                       });
        }

    private:
        template<typename TCallback>
        void ReadHeaderAsync(Session::Id sessionId, TCallback&& callback)
        {
            _sessionManager.ReadAsync(sessionId,
                                     asio::buffer(&_message.header,
                                                  sizeof(Message::Header)),
                                     [this,
                                      sessionId,
                                      callback = std::forward<TCallback>(callback)]
                                     (const ErrorCode& error,
                                      const size_t nBytesTransferred) mutable
                                     {
                                         OnHeaderRead(error,
                                                      nBytesTransferred,
                                                      sessionId,
                                                      std::move(callback));
                                     });
        }

        template<typename TCallback>
        void OnHeaderRead(const ErrorCode& error, const size_t nBytesTransferred, Session::Id sessionId, TCallback&& callback)
        {
            if (error)
            {
                std::cerr << "[" << sessionId << "] Failed to read header: " << error << "\n";
            }
            else
            {
                assert(nBytesTransferred == sizeof(Message::Header));
                assert(_message.header.size >= sizeof(Message::Header));

                // The size of payload is bigger than 0
                if (_message.header.size > sizeof(Message::Header))
                {
                    _message.payload.resize(_message.header.size - sizeof(Message::Header));
                    ReadPayloadAsync(sessionId,
                                     std::forward<TCallback>(callback));

                    return;
                }
            }

            OnMessageRead(error,
                          std::forward<TCallback>(callback));
        }

        template<typename TCallback>
        void ReadPayloadAsync(Session::Id sessionId, TCallback&& callback)
        {

            _sessionManager.ReadAsync(sessionId,
                                      asio::buffer(_message.payload.data(),
                                                   _message.payload.size()),
                                      [this,
                                       sessionId,
                                       callback = std::forward<TCallback>(callback)]
                                      (const ErrorCode& error,
                                       const size_t nBytesTransferred) mutable
                                      {
                                          OnPayloadRead(error,
                                                        nBytesTransferred,
                                                        sessionId,
                                                        std::move(callback));
                                      });
        }

        template<typename TCallback>
        void OnPayloadRead(const ErrorCode& error, const size_t nBytesTransferred, Session::Id sessionId, TCallback&& callback)
        {
            if (error)
            {
                std::cerr << "[" << sessionId << "] Failed to read payload: " << error << "\n";
            }
            else
            {
                assert(_message.payload.size() == nBytesTransferred);
            }

            OnMessageRead(error, 
                          sessionId, 
                          std::forward<TCallback>(callback));
        }

        template<typename TCallback>
        void OnMessageRead(const ErrorCode& error, Session::Id sessionId, TCallback&& callback)
        {
            if (!callback(error, sessionId, _message))
            {
                return;
            }

            asio::post(_strand,
                       [this,
                        sessionId,
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           PushMessage(sessionId, 
                                       std::move(callback));
                       });
        }

        template<typename TCallback>
        void PushMessage(Session::Id sessionId, TCallback&& callback)
        {
            _buffer.emplace(sessionId, std::move(_message));

            ReadHeaderAsync(sessionId,
                            std::forward<TCallback>(callback));
        }

    private:
        SessionManager&             _sessionManager;
        OwnedMessage::Buffer        _buffer;
        Strand                      _strand;
        Message                     _message;

    };
}
