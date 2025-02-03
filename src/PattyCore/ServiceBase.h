#pragma once

#include "Session.h"

namespace PattyCore
{
    /*-------------------*
     *    ServiceBase    *
     *-------------------*/

    class ServiceBase
    {
    public:
        /*-----------------------*
         *    ThreadPoolGroup    *
         *-----------------------*/

        class ThreadPoolGroup
        {
        public:
            struct Info
            {
                uint8_t     numSocketThreads;
                uint8_t     numSessionThreads;
                uint8_t     numMessageThreads;
                uint8_t     numTaskThreads;
            };

        public:
            ThreadPoolGroup(const Info& info);

            void Stop();
            void Join();

            ThreadPool&     GetSocketGroup();
            ThreadPool&     GetSessionGroup();
            ThreadPool&     GetMessageGroup();
            ThreadPool&     GetTaskGroup();

        private:
            ThreadPool      mSocketGroup;   // 소켓 입출력 스레드
            ThreadPool      mSessionGroup;  // 세션 관리 스레드
            ThreadPool      mMessageGroup;  // 메시지 처리 스레드
            ThreadPool      mTaskGroup;     // 범용 비동기 작업 스레드

            WorkGrd         mSocketGrd;
            WorkGrd         mSessionGrd;
            WorkGrd         mMessageGrd;
            WorkGrd         mTaskGrd;
        };

    protected:
        using OwnedMessage = Session::OwnedMessage;

    public:
        ServiceBase(const ThreadPoolGroup::Info& threadsInfo);
        virtual ~ServiceBase();

        void Stop();
        void Join();

    protected:
        virtual void OnSessionRegistered(Session::Ptr session) {}
        virtual void OnSessionUnregistered(Session::Ptr session) {}
        virtual void OnMessageReceived(OwnedMessage ownedMsg) {}

        void CreateSession(Tcp::socket&& socket);
        void BroadcastMessageAsync(Message&& msg, Session::Ptr ignored = nullptr);

    private:
        Session::Id AssignId() const;
        void DispatchReceivedMessage(OwnedMessage&& ownedMsg);

        void OnSessionCreated(Session::Ptr session);
        void RegisterSession(Session::Ptr session);
        void OnSessionClosed(const ErrCode& errCode, Session::Ptr session);
        void UnregisterSession(Session::Ptr session);

    protected:
        ThreadPoolGroup     mThreadPoolGroup;

        Session::Map        mSessionMap;
        Strand              mSessionStrand;
    };
}
