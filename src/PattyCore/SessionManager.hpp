#pragma once

#include <PattyCore/Session.hpp>

namespace PattyCore
{
    /*----------------------*
     *    SessionManager    *
     *----------------------*/

    class SessionManager
    {
    public:
        using SessionMap    = std::unordered_map<Session::Id, Session::Pointer>;

    public:
        SessionManager(ThreadPool& workers)
            : _strand(asio::make_strand(workers))
        {}

        template<typename TCallback>
        void RegisterAsync(Session::Pointer pSession, TCallback&& callback)
        {
            asio::post(_strand,
                       [this,
                        pSession = std::move(pSession),
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           Register(std::move(pSession),
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

        template<typename TBuffer, typename TCallback>
        void WriteAsync(Session::Id id, TBuffer&& buffer, TCallback&& callback)
        {
            asio::post(_strand,
                       [this,
                        id,
                        buffer = std::forward<TBuffer>(buffer),
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           assert(_sessions.count(id) == 1);

                           _sessions[id]->WriteAsync(std::move(buffer),
                                                     std::move(callback));
                       });
        }

        template<typename TBuffer, typename TCallback>
        void ReadAsync(Session::Id id, TBuffer&& buffer, TCallback&& callback)
        {
            asio::post(_strand,
                       [this,
                        id,
                        buffer = std::forward<TBuffer>(buffer),
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           assert(_sessions.count(id) == 1);

                           _sessions[id]->ReadAsync(std::move(buffer),
                                                    std::move(callback));
                       });
        }

        template<typename TCallback>
        void CloseAsync(Session::Id id, TCallback&& callback)
        {
            asio::post(_strand,
                       [this,
                        id,
                        callback = std::forward<TCallback>(callback)]
                       () mutable
                       {
                           assert(_sessions.count(id) == 1);

                           _sessions[id]->CloseAsync(std::move(callback));
                       });
        }

    private:
        template<typename TCallback>
        void Register(Session::Pointer pSession, TCallback&& callback)
        {
            const Session::Id id = pSession->GetId();
            assert(_sessions.count(id) == 0);

            _sessions[id] = std::move(pSession);

            callback(id);
        }

        template<typename TCallback>
        void Unregister(Session::Id id, TCallback&& callback)
        {
            assert(_sessions.count(id) == 1);

            Session::Pointer pSession = std::move(_sessions[id]);
            _sessions.erase(id);

            callback(std::move(pSession));
        }

        template<typename TCallback>
        void Clear(TCallback&& callback)
        {
            _sessions.clear();

            callback();
        }

    private:
        SessionMap      _sessions;
        Strand          _strand;

    };
}
