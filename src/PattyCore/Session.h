#pragma once

#include "Message.h"

namespace PattyCore
{
    /*---------------*
     *    Session    *
     *---------------*/

    class Session
        : public std::enable_shared_from_this<Session>
    {
    public:
        using Id = uint32_t;
        using Ptr = SPtr<Session>;
        using Map = std::unordered_map<Id, Ptr>;
        using OwnedMessage = OwnedMessage<Session>;
        using OnClosed = std::function<void(const ErrCode&, Ptr)>;
        using OnReceived = std::function<void(OwnedMessage&&)>;

    public:
        ~Session();

        static Ptr Create(Tcp::socket&& socket,
                          const Id id,
                          OnClosed onClosed,
                          Strand&& writeStrand,
                          OnReceived onReceived);

        void SendAsync(Message&& sendMsg);

        void Close();

        Id GetId() const noexcept;
        const Tcp::endpoint& GetEndpoint() const noexcept;

        friend std::ostream& operator<<(std::ostream& os, const Session& session);

    private:
        Session(Tcp::socket&& socket,
                const Id id,
                OnClosed&& onClosed,
                Strand&& writeStrand,
                OnReceived&& onReceived);

        void WriteHeaderAsync(Message::Ptr msg);
        void OnHeaderWritten(const ErrCode& errCode, const size_t numBytes, Message::Ptr msg);
        void WritePayloadAsync(Message::Ptr msg);
        void OnPayloadWritten(const ErrCode& errCode, const size_t numBytes, Message::Ptr msg);
        void OnMessageWritten(const ErrCode& errCode);

        void ReceiveAsync(Ptr self);
        void ReadHeaderAsync();
        void OnHeaderRead(const ErrCode& errCode, const size_t numBytes);
        void ReadPayloadAsync();
        void OnPayloadRead(const ErrCode& errCode, const size_t numBytes);
        void OnMessageRead(const ErrCode& errCode);

    private:
        Tcp::socket             mSocket;
        SMutex                  mSocketLock;

        const Id                mId;
        const Tcp::endpoint     mEndpoint;

        OnClosed                mOnClosed;

        Strand                  mWriteStrand;
        OwnedMessage            mReadMsg;
        OnReceived              mOnReceived;
    };
}
