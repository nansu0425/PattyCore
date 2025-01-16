#pragma once

#include <PattyCore/Sender.hpp>

namespace PattyCore
{
    /*---------------------*
     *    SenderManager    *
     *---------------------*/

    class SenderManager
    {
    public:
        using SenderMap             = std::unordered_map<Session::Id, Sender::Pointer>;

    public:
        SenderManager(ThreadPool& workers)
            : _strand(asio::make_strand(workers))
        {}

        template<typename TCallback>
        void RegisterAsync(Sender::Pointer pSender, TCallback&& callback)
        {
            asio::post(_strand,
                       [this,
                        pSender = std::move(pSender),
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           Register(std::move(pSender),
                                    std::move(callback));
                       });
        }

        template<typename TCallback>
        void UnregisterAsync(Session::Id id, TCallback&& callback)
        {
            asio::post(_strand,
                       [this, 
                        id, 
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           Unregister(id, 
                                      std::move(callback));
                       });
        }

        template<typename TCallback>
        void ClearAsync(TCallback&& callback)
        {
            asio::post(_strand,
                       [this,
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           Clear(std::move(callback));
                       });
        }

        template<typename TMessage, typename TCallback>
        void SendMessageAsync(Session::Id id, TMessage&& message, TCallback callback)
        {
            asio::post(_strand,
                       [this,
                        id,
                        message = std::forward<TMessage>(message),
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           assert(_senders.count(id) == 1);

                           _senders[id]->SendMessageAsync(std::move(message),
                                                          std::move(callback));
                       });
        }

    private:
        template<typename TCallback>
        void Register(Sender::Pointer pSender, TCallback&& callback)
        {
            const Session::Id id = pSender->GetSessionId();
            assert(_senders.count(id) == 0);

            _senders[id] = std::move(pSender);

            callback(id);
        }

        template<typename TCallback>
        void Unregister(Session::Id id, TCallback&& callback)
        {
            assert(_senders.count(id) == 1);

            Sender::Pointer pSender = std::move(_senders[id]);
            _senders.erase(id);

            callback(std::move(pSender));
        }

        template<typename TCallback>
        void Clear(TCallback&& callback)
        {
            _senders.clear();

            callback();
        }

    private:
        SenderMap               _senders;
        Strand                  _strand;

    };
}
