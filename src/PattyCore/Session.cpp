#include "Pch.h"
#include "Session.h"

namespace PattyCore
{
    Session::~Session()
    {
        std::cout << *this << " Session destroyed: " << GetEndpoint() << "\n";
    }

    Session::Ptr Session::Create(Tcp::socket&& socket,
                                 const Id id,
                                 OnClosed onClosed,
                                 Strand&& writeStrand,
                                 OnReceived onReceived)
    {
        Ptr newSession = Ptr(new Session(std::move(socket),
                                         id,
                                         std::move(onClosed),
                                         std::move(writeStrand),
                                         std::move(onReceived)));
        newSession->ReceiveAsync(newSession);

        return newSession;
    }

    void Session::SendAsync(Message&& sendMsg)
    {
        Message::Ptr msg = std::make_unique<Message>(std::move(sendMsg));

        asio::post(mWriteStrand,
                   [self = shared_from_this(), msg = std::move(msg)]() mutable
                   {
                       self->WriteHeaderAsync(std::move(msg));
                   });
    }

    void Session::Close()
    {
        ErrCode errCode;

        {
            SMutexULock lock(mSocketLock);

            if (!mSocket.is_open())
            {
                return;
            }

            mSocket.close(errCode);
        }

        mOnClosed(errCode, shared_from_this());
    }

    Session::Id Session::GetId() const noexcept
    {
        return mId;
    }

    const Tcp::endpoint& Session::GetEndpoint() const noexcept
    {
        return mEndpoint;
    }

    std::ostream& operator<<(std::ostream& os, const Session& session)
    {
        os << "[" << session.GetId() << "]";

        return os;
    }

    Session::Session(Tcp::socket&& socket,
                     const Id id,
                     OnClosed&& onClosed,
                     Strand&& writeStrand,
                     OnReceived&& onReceived)
        : mSocket(std::move(socket))
        , mId(id)
        , mEndpoint(mSocket.remote_endpoint())
        , mOnClosed(std::move(onClosed))
        , mWriteStrand(std::move(writeStrand))
        , mOnReceived(std::move(onReceived))
    {
        std::cout << *this << " Session created: " << GetEndpoint() << "\n";
    }

    void Session::WriteHeaderAsync(Message::Ptr msg)
    {
        SMutexSLock lock(mSocketLock);
        Message::Header& header = msg->header;

        asio::async_write(mSocket,
                          asio::buffer(&header, sizeof(Message::Header)),
                          asio::bind_executor(mWriteStrand,
                                              [self = shared_from_this(), msg = std::move(msg)]
                                              (const ErrCode& errCode, const size_t numBytes) mutable
                                              {
                                                  self->OnHeaderWritten(errCode, numBytes, std::move(msg));
                                              }));
    }

    void Session::OnHeaderWritten(const ErrCode& errCode, const size_t numBytes, Message::Ptr msg)
    {
        if (errCode)
        {
            std::cerr << *this << " Failed to write header : " << errCode << "\n";
        }
        else
        {
            assert(sizeof(Message::Header) == numBytes);

            // The size of payload is bigger than 0
            if (msg->header.size > sizeof(Message::Header))
            {
                WritePayloadAsync(std::move(msg));

                return;
            }
        }

        OnMessageWritten(errCode);
    }

    void Session::WritePayloadAsync(Message::Ptr msg)
    {
        SMutexSLock lock(mSocketLock);
        Message::Payload& payload = msg->payload;

        asio::async_write(mSocket,
                          asio::buffer(payload),
                          [self = shared_from_this(), msg = std::move(msg)]
                          (const ErrCode& errCode, const size_t numBytes) mutable
                          {
                              self->OnPayloadWritten(errCode, numBytes, std::move(msg));
                          });
    }

    void Session::OnPayloadWritten(const ErrCode& errCode, const size_t numBytes, Message::Ptr msg)
    {
        if (errCode)
        {
            std::cerr << *this << " Failed to write payload: " << errCode << "\n";
        }
        else
        {
            assert(numBytes == msg->payload.size());
        }

        OnMessageWritten(errCode);
    }

    void Session::OnMessageWritten(const ErrCode& errCode)
    {
        if (errCode)
        {
            Close();
        }
    }

    void Session::ReceiveAsync(Ptr self)
    {
        assert(mReadMsg.owner == nullptr);
        mReadMsg.owner = std::move(self);

        ReadHeaderAsync();
    }

    void Session::ReadHeaderAsync()
    {
        SMutexSLock lock(mSocketLock);

        asio::async_read(mSocket,
                         asio::buffer(&mReadMsg.msg.header, sizeof(Message::Header)),
                         [this](const ErrCode& errCode, const size_t numBytes)
                         {
                             OnHeaderRead(errCode, numBytes);
                         });
    }

    void Session::OnHeaderRead(const ErrCode& errCode, const size_t numBytes)
    {
        if (errCode)
        {
            std::cerr << *this << " Failed to read header: " << errCode << "\n";
        }
        else
        {
            assert(numBytes == sizeof(Message::Header));
            assert(mReadMsg.msg.header.size >= sizeof(Message::Header));

            // The size of payload is bigger than 0
            if (mReadMsg.msg.header.size > sizeof(Message::Header))
            {
                mReadMsg.msg.payload.resize(mReadMsg.msg.header.size - sizeof(Message::Header));
                ReadPayloadAsync();

                return;
            }
        }

        OnMessageRead(errCode);
    }

    void Session::ReadPayloadAsync()
    {
        SMutexSLock lock(mSocketLock);

        asio::async_read(mSocket,
                         asio::buffer(mReadMsg.msg.payload),
                         [this](const ErrCode& errCode, const size_t numBytes)
                         {
                             OnPayloadRead(errCode, numBytes);
                         });
    }

    void Session::OnPayloadRead(const ErrCode& errCode, const size_t numBytes)
    {
        if (errCode)
        {
            std::cerr << *this << " Failed to read payload: " << errCode << "\n";
        }
        else
        {
            assert(mReadMsg.msg.payload.size() == numBytes);
        }

        OnMessageRead(errCode);
    }

    void Session::OnMessageRead(const ErrCode& errCode)
    {
        Ptr self = std::move(mReadMsg.owner);

        if (errCode)
        {
            Close();

            return;
        }

        mReadMsg.owner = self;
        mOnReceived(std::move(mReadMsg));

        ReceiveAsync(std::move(self));
    }
}
