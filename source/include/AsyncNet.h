
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

            int          localPort;
            string       remoteIP;
            int          remotePort;
            union
            {
                bool     autoDestroy;
                int      listenPort;
            };
        };

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
            //不处理重复的注册
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

        void destroySession(INetSession_SYS* session)
        {}

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
            NLOG_INFO("有连接: %s : %d  进入", remoteIP, remotePort);
            SessionContext* ctx = (SessionContext*)listenContext;

            auto it = m_listenings.find(ctx->listenPort);
            if (it == m_listenings.end())
            {
                NLOG_INFO("没有找到监听者，不接受连接 %s : %d", remoteIP, remotePort);
                return false;
            }

            auto factory = it->second.second;

            INetSession_SYS* session = factory->CreateSession();
            if (!session)
            {
                NLOG_ERROR("系统错误：监听者工厂对象不存在！ %s : %d", localIP, localPort);
                return false;
            }

            SessionContext* sessionContext = new SessionContext();

            sessionContext->sessionName = it->second.first;
            sessionContext->session = session;
            sessionContext->autoDestroy = true;

            sessionContext->localPort = localPort;
            sessionContext->remotePort = remotePort;
            sessionContext->remoteIP = remoteIP;

            *(SessionContext**)newContext = sessionContext;
            return true;
        }

        void AcceptFail(void* listenContext, void* newContext) OVERRIDE
        {
            //NLOG_WARNING("接受连接： %s : %d 失败！", ip, port);
            SessionContext* ctx = (SessionContext*)newContext;
            destroySession(ctx->session);
            delete ctx;
        }

        void OnNetAccepted(H64K netHandle, void* listenContext, void* newContext) OVERRIDE
        {
            //NLOG_INFO("接受连接: %s : %d \r\n", ip, port);

            SessionContext* ctx = (SessionContext*)newContext;
            interlockedInc(&totalStart);
            ctx->session->Start(netHandle, this,
                "127.0.0.1",
                ctx->localPort,
                ctx->remoteIP.c_str(),
                ctx->remotePort,
                m_iocpNet.getId(netHandle));

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

            // stop all net operations
            session->Stop();
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

                //不要释放对象
                //factory->DestroySession(session);
            }

            delete sessionCtx;
        }
    };
}

#endif