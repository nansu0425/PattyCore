#pragma once

#include <PattyCore/Message.hpp>

namespace PattyCore
{
    /*---------------------*
     *    MessageSender    *
     *---------------------*/

    class MessageSender
    {
    public:
        using Buffer        = Message::Buffer;

        MessageSender(ThreadPool& workers,
                      Tcp::socket& socket,
                      Strand& socketStrand)
            : m_socket(socket)
            , m_socketStrand(socketStrand)
            , m_strand(asio::make_strand(workers))
            , m_isWriting(false)
        {}

        template<typename TMessage>
        void SendAsync(TMessage&& message)
        {
            asio::post(m_strand,
                       [this, 
                       message = std::forward<TMessage>(message)]() mutable
                       {
                           PushMessage(std::move(message));
                       });
        }

    private:
        template<typename TMessage>
        void PushMessage(TMessage&& message)
        {
            m_buffer.emplace(std::forward<TMessage>(message));

            WriteMessageAsync();
        }

        void WriteMessageAsync()
        {
            if (m_isWriting ||
                m_buffer.empty())
            {
                return;
            }

            m_message = std::move(m_buffer.front());
            m_buffer.pop();

            asio::post(m_socketStrand,
                       [this]()
                       {
                           WriteHeaderAsync();
                       });

            m_isWriting = true;
        }

        void WriteHeaderAsync()
        {
            asio::async_write(m_socket,
                              asio::buffer(&m_message.header,
                                           sizeof(Message::Header)),
                              [this](const ErrorCode& error,
                                     const size_t nBytesTransferred)
                              {
                                  OnWriteHeaderCompleted(error, nBytesTransferred);
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
                if (m_message.header.size > sizeof(Message::Header))
                {
                    asio::post(m_socketStrand,
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
                              asio::buffer(m_message.payload.data(),
                                           m_message.payload.size()),
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
                assert(nBytesTransferred == m_message.payload.size());
            }

            asio::post(_sendStrand,
                       [pSelf = shared_from_this(), error]
                       {
                           pSelf->OnWriteMessageCompleted(error);
                       });
        }

        void OnWriteMessageCompleted(const ErrorCode& error)
        {
            m_isWriting = false;

            if (error)
            {
                CloseAsync();
                return;
            }

            WriteMessageAsync();
        }

    private:
        Tcp::socket&        m_socket;
        Strand&             m_socketStrand;

        Buffer              m_buffer;
        Strand              m_strand;
        Message             m_message;
        bool                m_isWriting;
    };
}
