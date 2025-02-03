#include "Pch.h"
#include "ServiceBase.h"

namespace PattyCore
{
    ServiceBase::ThreadPoolGroup::ThreadPoolGroup(const Info& info)
        : mSocketGroup(info.numSocketThreads)
        , mSessionGroup(info.numSessionThreads)
        , mMessageGroup(info.numMessageThreads)
        , mTaskGroup(info.numTaskThreads)
        , mSocketGrd(asio::make_work_guard(mSocketGroup))
        , mSessionGrd(asio::make_work_guard(mSessionGroup))
        , mMessageGrd(asio::make_work_guard(mMessageGroup))
        , mTaskGrd(asio::make_work_guard(mTaskGroup))
    {}

    void ServiceBase::ThreadPoolGroup::Stop()
    {
        mSocketGroup.stop();
        mSessionGroup.stop();
        mMessageGroup.stop();
        mTaskGroup.stop();
    }

    void ServiceBase::ThreadPoolGroup::Join()
    {
        mSocketGroup.join();
        mSessionGroup.join();
        mMessageGroup.join();
        mTaskGroup.join();
    }

    ThreadPool& ServiceBase::ThreadPoolGroup::GetSocketGroup() 
    { 
        return mSocketGroup; 
    }

    ThreadPool& ServiceBase::ThreadPoolGroup::GetSessionGroup() 
    { 
        return mSessionGroup; 
    }

    ThreadPool& ServiceBase::ThreadPoolGroup::GetMessageGroup() 
    { 
        return mMessageGroup; 
    }

    ThreadPool& ServiceBase::ThreadPoolGroup::GetTaskGroup() 
    { 
        return mTaskGroup; 
    }

    ServiceBase::ServiceBase(const ThreadPoolGroup::Info& info)
        : mThreadPoolGroup(info)
        , mSessionStrand(asio::make_strand(mThreadPoolGroup.GetSessionGroup()))
    {}

    ServiceBase::~ServiceBase() {}

    void ServiceBase::Stop()
    {
        mThreadPoolGroup.Stop();
    }

    void ServiceBase::Join()
    {
        mThreadPoolGroup.Join();
    }

    void ServiceBase::CreateSession(Tcp::socket&& socket)
    {
        auto onSessionClosed = [this](const ErrCode& errCode, Session::Ptr session)
            {
                OnSessionClosed(errCode, std::move(session));
            };

        auto onMessageReceived = [this](OwnedMessage&& ownedMsg)
            {
                DispatchReceivedMessage(std::move(ownedMsg));
            };

        Session::Ptr session = Session::Create(std::move(socket),
                                               AssignId(),
                                               std::move(onSessionClosed),
                                               asio::make_strand(mThreadPoolGroup.GetSocketGroup()),
                                               std::move(onMessageReceived));

        OnSessionCreated(std::move(session));
    }

    void ServiceBase::BroadcastMessageAsync(Message&& msg, Session::Ptr ignored)
    {
        const Session::Id ignoredId = (ignored) ? ignored->GetId() : -1;

        asio::post(mSessionStrand,
                   [this, msg = std::move(msg), ignoredId]()
                   {
                       for (auto& pair : mSessionMap)
                       {
                           if (pair.first == ignoredId)
                           {
                               continue;
                           }

                           pair.second->SendAsync(Message(msg));
                       }
                   });
    }

    Session::Id ServiceBase::AssignId() const
    {
        static std::atomic<Session::Id> id = 10000;
        Session::Id assigned = id.fetch_add(1);

        return assigned;
    }

    void ServiceBase::DispatchReceivedMessage(OwnedMessage&& ownedMsg)
    {
        asio::post(mThreadPoolGroup.GetMessageGroup(),
                   [this, ownedMsg = std::move(ownedMsg)]() mutable
                   {
                       OnMessageReceived(std::move(ownedMsg));
                   });
    }

    void ServiceBase::OnSessionCreated(Session::Ptr session)
    {
        asio::post(mSessionStrand,
                   [this, session = std::move(session)]() mutable
                   {
                       RegisterSession(std::move(session));
                   });
    }

    void ServiceBase::RegisterSession(Session::Ptr session)
    {
        const Session::Id id = session->GetId();

        assert(mSessionMap.count(id) == 0);
        mSessionMap[id] = std::move(session);

        asio::post(mThreadPoolGroup.GetSessionGroup(),
                   [this, session = mSessionMap[id]]() mutable
                   {
                       OnSessionRegistered(std::move(session));
                   });
    }

    void ServiceBase::OnSessionClosed(const ErrCode& errCode, Session::Ptr session)
    {
        if (errCode)
        {
            std::cerr << *session << " Failed to close session: " << errCode << "\n";
        }

        asio::post(mSessionStrand,
                   [this, session = std::move(session)]() mutable
                   {
                       UnregisterSession(std::move(session));
                   });
    }

    void ServiceBase::UnregisterSession(Session::Ptr session)
    {
        const Session::Id id = session->GetId();

        assert(mSessionMap.count(id) == 1);
        mSessionMap.erase(id);

        asio::post(mThreadPoolGroup.GetSessionGroup(),
                   [this, session = std::move(session)]() mutable
                   {
                       OnSessionUnregistered(std::move(session));
                   });
    }
}
