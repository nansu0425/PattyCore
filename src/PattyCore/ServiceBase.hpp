﻿#pragma once

#include <PattyCore/Session.hpp>

namespace PattyCore
{
    /*-------------------*
     *    ServiceBase    *
     *-------------------*/

    class ServiceBase
    {
    protected:
        using TickRate              = uint32_t;
        using OwnedMessage          = Session::OwnedMessage;

    private:
        using SessionMap = std::unordered_map<Session::Id, Session::Pointer>;

    public:
        ServiceBase(size_t nWorkers)
            : _workers(nWorkers)
            , _workGuard(asio::make_work_guard(_workers))
            , _sessionsStrand(asio::make_strand(_workers))
            , _tickRateTimer(_workers)
            , _tickRate(0)
        {}

        virtual ~ServiceBase()
        {}

        void StopWorkers()
        {
            _workers.stop();
        }

        void JoinWorkers()
        {
            _workers.join();
        }

    protected:
        virtual void OnSessionRegistered(Session::Pointer pSession) {}
        virtual void OnSessionUnregistered(Session::Pointer pSession) {}
        virtual void HandleReceivedMessage(OwnedMessage ownedMessage) {}
        virtual void OnTickRateMeasured(const TickRate tickRate) {}

        void Run()
        {
            WaitTickRateTimerAsync();
            HandleReceivedMessagesAsync();
        }

        void CreateSession(Tcp::socket&& socket)
        {
            auto onSessionClosed = [this](const ErrorCode& error,
                                          Session::Pointer pSession) mutable
                                   {
                                       OnSessionClosed(error, 
                                                       std::move(pSession));
                                   };

            Session::Pointer pSession = Session::Create(_workers,
                                                        std::move(socket),
                                                        AssignId(),
                                                        std::move(onSessionClosed),
                                                        _receiveBuffer);

            asio::post(_sessionsStrand,
                       [this, pSession = std::move(pSession)]() mutable
                       {
                           RegisterSession(std::move(pSession));
                       });
        }

        void BroadcastMessageAsync(Message&& message, Session::Pointer pIgnored = nullptr)
        {
            const Session::Id ignored = (pIgnored) ? pIgnored->GetId() : -1;

            asio::post(_sessionsStrand,
                       [this, 
                        message = std::move(message),
                        ignored]()
                       {
                           for (auto& pair : _sessions)
                           {
                               if (pair.first == ignored)
                               {
                                   continue;
                               }

                               pair.second->SendAsync(pair.second, Message(message));
                           }
                       });
        }

    private:
        Session::Id AssignId()
        {
            static Session::Id id = 10000;
            Session::Id assignedId = id;
            ++id;

            return assignedId;
        }

        void HandleReceivedMessagesAsync()
        {
            asio::post([this]()
                       {
                           HandleReceivedMessages();
                       });
        }

        void HandleReceivedMessages()
        {
            OwnedMessage ownedMessage;

            while (_receiveBuffer.Pop(ownedMessage))
            {
                asio::post([this, ownedMessage = std::move(ownedMessage)]() mutable
                           {
                               HandleReceivedMessage(std::move(ownedMessage));
                           });
            }

            _tickRate.fetch_add(1);
            HandleReceivedMessagesAsync();
        }

        void OnSessionClosed(const ErrorCode& error, Session::Pointer pSession)
        {
            if (error)
            {
                std::cerr << *pSession << " Failed to close session: " << error << "\n";
            }

            asio::post(_sessionsStrand,
                       [this, 
                        pSession = std::move(pSession)]() mutable
                       {
                           UnregisterSession(std::move(pSession));
                       });
        }

        void RegisterSession(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();
            
            assert(_sessions.count(id) == 0);
            _sessions[id] = std::move(pSession);

            asio::post([this, 
                        pSession = _sessions[id]]() mutable
                       {
                           OnSessionRegistered(std::move(pSession));
                       });
        }

        void UnregisterSession(Session::Pointer pSession)
        {
            const Session::Id id = pSession->GetId();

            assert(_sessions.count(id) == 1);
            _sessions.erase(id);

            asio::post([this, 
                        pSession = std::move(pSession)]() mutable
                       {
                           OnSessionUnregistered(std::move(pSession));
                       });
        }

        void WaitTickRateTimerAsync()
        {
            _tickRateTimer.expires_after(Seconds(1));
            _tickRateTimer.async_wait([this](const ErrorCode& error)
                                      {
                                          OnTickRateTimerExpired(error);
                                      });
        }

        void OnTickRateTimerExpired(const ErrorCode& error)
        {
            if (error)
            {
                std::cerr << "[TICK_RATE_TIMER] Failed to wait: " << error << "\n";
                return;
            }

            WaitTickRateTimerAsync();

            const TickRate tickRate = _tickRate.exchange(0);
            OnTickRateMeasured(tickRate);
        }

    protected:
        ThreadPool                      _workers;
        WorkGuard                       _workGuard;
        SessionMap                      _sessions;
        Strand                          _sessionsStrand;

        Timer                           _tickRateTimer;
        std::atomic<TickRate>           _tickRate;

        OwnedMessage::Buffer            _receiveBuffer;

    };
}
