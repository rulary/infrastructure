
#ifndef _ASYNCNET_H_
#define _ASYNCNET_H_

#include "../IOCPNet/IOCPNet.h"

#include "AsyncNetType.h"
#include "NetHandler.h"

#include "ThreadSync.h"
#include "NetLog.h"

#include <map>
#include <string>
#include <memory>
#include <list>
#include <utility>
#include <assert.h>

namespace AsyncNeting
{
    using namespace std;

    class AsyncNet : private INetHandler
    {
        ExLock            m_lock;
        IOCPNet           m_iocpNet;

        map<string, shared_ptr<INetSessionFactory>>              m_factories;
        map<int, pair<string, shared_ptr<INetSessionFactory>>>    m_listenings;
        //list<INetSession*>                         m_allSessions;

        long_t totalStoped;
        long_t totalStart;

        struct SessionContext
        {
            string       sessionName;
            INetSession_SYS* session;

            string       localIP;
            int          localPort;
            string       remoteIP;
            int          remotePort;
            union
            {
                bool     autoDestroy;
                int      listenPort;
            };
        };

        void _destroySession(INetSession_SYS* session)
        {}

        SessionContext* _createSessionContext()
        {
            return new SessionContext();
        }

        void _deleteSessionContext(SessionContext* ctx)
        {
            delete ctx;
        }

    public:
        AsyncNet()
            :m_iocpNet((INetHandler*)this)
        {
            totalStart = 0;
            totalStoped = 0;
        }

    public:
        void registerNetSession(const char* szName, shared_ptr<INetSessionFactory> factory)
        {
            Locker lock(&m_lock);
            //overwite duplcate key value
            string str(szName);
            m_factories[str] = factory;
        }

        INetSession* startSession(const char* szName, const char* szIP, const int iPort, bool autoDestroy = true)
        {
            EnterLock(&m_lock);

            string str(szName);
            auto it = m_factories.find(str);
            if (it == m_factories.end())
            {
                LeaveLock(&m_lock);
                return nullptr;
            }

            shared_ptr<INetSessionFactory> factory = it->second;

            LeaveLock(&m_lock);

            INetSession_SYS* session = factory->CreateSession();
            if (!session)
                return nullptr;

            SessionContext* sessionContext = new SessionContext();

            sessionContext->sessionName = str;
            sessionContext->session = session;
            sessionContext->autoDestroy = autoDestroy;

            sessionContext->localPort = 0;
            sessionContext->remotePort = iPort;
            sessionContext->remoteIP = szIP;

            NETHANDLE h = m_iocpNet.connect(szIP, iPort, sessionContext);
            if (h == INVALID_NETHANDLE)
            {
                factory->DestroySession(session);
                delete sessionContext;
                return nullptr;
            }

            interlockedInc(&totalStart);

            string localIP("127.0.0.1");
            m_iocpNet.getLocalAddr(h, localIP, sessionContext->localPort);

            session->Start(h, this,
                localIP.c_str(),
                sessionContext->localPort,
                sessionContext->remoteIP.c_str(),
                sessionContext->remotePort,
                m_iocpNet.getId(h));

            //m_allSessions.push_back(session);
            m_iocpNet.startReceiv(h);
            return session->getNetSessionInterface();
        }

        

        bool listen(const char* szSessionName, const int iPort, const char* szIP = nullptr)
        {
            auto it = m_listenings.find(iPort);
            if (it != m_listenings.end())
                return false;

            string str(szSessionName);

            EnterLock(&m_lock);
            auto itFactory = m_factories.find(str);
            if (itFactory == m_factories.end())
            {
                LeaveLock(&m_lock);
                return false;
            }
            auto factory = itFactory->second;
            LeaveLock(&m_lock);

            //TODO: RWLock protectint m_listenings from access 
            m_listenings[iPort] = pair<string, shared_ptr<INetSessionFactory>>(str, factory);

            SessionContext* ctx = new SessionContext();
            ctx->listenPort = iPort;
            ctx->sessionName = szSessionName;
            ctx->session = nullptr;

            NETHANDLE h = m_iocpNet.listen(iPort, ctx, szIP);
            if (h != INVALID_NETHANDLE)
            {
                m_iocpNet.closeHandle(h);
                return true;
            }

            delete ctx;
            m_listenings.erase(iPort);
            return false;
        }

        void closeNetHandle(NETHANDLE h)
        {
            m_iocpNet.closeHandle(h, true);
        }

        bool send(NETHANDLE h, char* buff, int dataLen)
        {
            return m_iocpNet.send(h, buff, dataLen);
        }

    public:
        bool PrepareToAccept(const char* localIP, int localPort, const char* remoteIP, int remotePort, void* listenContext, void** newContext) OVERRIDE
        {
            NLOG_INFO("new connection : R<%s : %d> incoming", remoteIP, remotePort);
            SessionContext* ctx = (SessionContext*)listenContext;

            assert(ctx->listenPort == localPort);

            auto it = m_listenings.find(ctx->listenPort);
            if (it == m_listenings.end())
            {
                NLOG_ERROR("can not find the session listener on  L<%s :  %d£¬remote connection R<%s : %d> refused", localIP, localPort, remoteIP, remotePort);
                return false;
            }

            auto factory = it->second.second;

            INetSession_SYS* session = factory->CreateSession();
            if (!session)
            {
                NLOG_ERROR("failed to create session object for session L<%s : %d> <= R<%s : %d>", localIP, localPort, remoteIP, remotePort);
                return false;
            }

            SessionContext* sessionContext = _createSessionContext();

            sessionContext->sessionName = it->second.first;
            sessionContext->session = session;
            sessionContext->autoDestroy = true;

            sessionContext->localIP = localIP;
            sessionContext->localPort = localPort;
            sessionContext->remotePort = remotePort;
            sessionContext->remoteIP = remoteIP;

            *(SessionContext**)newContext = sessionContext;
            return true;
        }

        void AcceptFail(void* listenContext, void* newContext) OVERRIDE
        {
            SessionContext* ctx = (SessionContext*)newContext;
            _destroySession(ctx->session);
            _deleteSessionContext(ctx);
        }

        void OnNetAccepted(H64K netHandle, void* listenContext, void* newContext) OVERRIDE
        {
            // for debug
            interlockedInc(&totalStart);

            SessionContext* ctx = (SessionContext*)newContext;

            // raise session start event, inform our user that they can use all net operations now (meanwhile prepare to receive net data from now)
            ctx->session->Start(netHandle,      // pass the net handle to that session
                this,
                ctx->localIP.c_str(),
                ctx->localPort,
                ctx->remoteIP.c_str(),
                ctx->remotePort,
                m_iocpNet.getId(netHandle));    //use net Id as session Id

            // inform net system to accept net data
            m_iocpNet.startReceiv(netHandle);
        }

        int OnReceiv(void* dataBuff, int dataLen, void* context, bool canreserved) OVERRIDE
        {
            SessionContext* sessionCtx = (SessionContext*)context;
            INetSession_SYS* session = sessionCtx->session;

            return session->OnReceiv((char*)dataBuff, dataLen, canreserved);
        }

        void OnNetClose(int closeReasson, void* context) OVERRIDE
        {
            SessionContext* sessionCtx = (SessionContext*)context;
            INetSession_SYS* session = sessionCtx->session;

            session->OnNetClose();
        }

        void OnNetConnected(int connectType, void* context) OVERRIDE
        {
            SessionContext* sessionCtx = (SessionContext*)context;
            INetSession_SYS* session = sessionCtx->session;

            session->OnNetOpen();
        }

        void OnNetDestroy(void* context) OVERRIDE
        {
            SessionContext* sessionCtx = (SessionContext*)context;
            INetSession_SYS* session = sessionCtx->session;

            if (!session)
            {
                delete sessionCtx;
                return;
            }

            // stop the session; all net operations of that session should not luanche after this event
            session->Stop();

            // for debug
            interlockedInc(&totalStoped);

            if (sessionCtx->autoDestroy)
            {
                EnterLock(&m_lock);
                string str(sessionCtx->sessionName);
                auto it = m_factories.find(str);

                if (it == m_factories.end())
                {
                    // should never been here
                    assert(false);
                    LeaveLock(&m_lock);
                    return;
                }

                shared_ptr<INetSessionFactory> factory = it->second;
                LeaveLock(&m_lock);

                //do not destroy the session here, sence it's not control by our handler sytem
                //factory->DestroySession(session);
            }

            delete sessionCtx;
        }
    };
}

#endif