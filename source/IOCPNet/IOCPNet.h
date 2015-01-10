#ifndef _IOCPNET_H_
#define _IOCPNET_H_

#include <Winsock2.h>
#include <mswsock.h>

#include "IOCPNetType.h"
#include "64KHandle.h"
#include "RundownProtect.h"
#include "IOCPUseHeaps.h"
#include "NetHandler.h"

#include "NetLog.h"



#include <string>
#include <list>
#include <assert.h>

#pragma warning(push)
#pragma warning(disable : 4355)

class IOCPNet : private I64kObjectManager
{
private:
    //temp implement
    HeapOfIOCP          m_heaps;

private:
    HANDLE              m_hIOCP;
    INetHandler         *m_NetHandler;

    std::list<HANDLE>   m_arrWorkThreads;
    std::list<HANDLE>   m_arrListenThreads;
    _64kHandle          m_64kHandle;

    long_t              m_nextHadndleId;

    LPFN_ACCEPTEX       m_lpfAcceptEx;

public:
    IOCPNet(INetHandler* netHandler)
        :m_hIOCP(NULL)
        , m_NetHandler(netHandler)
        , m_64kHandle(this)
        , m_nextHadndleId(-1)
    {
        SYSTEM_INFO sysinfo = { 0 };
        ::GetNativeSystemInfo(&sysinfo);

        DWORD dwWorkThread = sysinfo.dwNumberOfProcessors * 2 + 2;
        m_hIOCP = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, sysinfo.dwNumberOfProcessors + 2);

        _startWorkTrhead((int)dwWorkThread);
        //_startWorkTrhead(2);
        WSADATA wsaData;
        ::WSAStartup(MAKEWORD(2, 2), &wsaData);

        DWORD dwBytesRet;
        GUID  guidAcceptEx = WSAID_ACCEPTEX;
        m_lpfAcceptEx = nullptr;

        SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

        ::WSAIoctl(s,
            SIO_GET_EXTENSION_FUNCTION_POINTER,
            &guidAcceptEx,
            sizeof(guidAcceptEx),
            &m_lpfAcceptEx,
            sizeof(m_lpfAcceptEx),
            &dwBytesRet, NULL, NULL);

        ASSERT(m_lpfAcceptEx != nullptr);

        ::closesocket(s);
    }

    ~IOCPNet()
    {}

private:
    void _startWorkTrhead(int num)
    {
        for (int i = 0; i < num; i++)
        {
            HANDLE h = ::CreateThread(NULL, 0, &IOCPWorkThreadProc, this, 0, NULL);
            m_arrWorkThreads.push_back(h);
        }
    }

    inline 
    int closeSocket(SOCKET s)
    {
        NLOG_INFO("关闭套接字： %x",s);
        return ::closesocket(s);
    }

    inline
    char* _allocReceivBuff()
    {
        return m_heaps.allocReceivBuff();
    }
    inline
    void _releaseReceivBuff(char* buff)
    {
        m_heaps.releaseReceivBuff(buff);
    }
    inline
    char* _allocSendBuff(int len)
    {
        return m_heaps.allocSendBuff(len);
    }
    inline
    void _releaseSendBuff(char* buff)
    {
        m_heaps.releaseSendBuff(buff);
    }
    inline
    char* _allocAcceptExBuff()
    {
        return m_heaps.allocAcceptExBuff();
    }
    inline
    void _releaseAcceptExBuff(char* buff)
    {
        m_heaps.releaseAcceptExBuff(buff);
    }
    inline
    IOCPMSG* _allocPerIOData()
    {
        IOCPMSG* io = m_heaps.allocPerIOData();
        if(io)
            io->processed = 0;
        return io;
    }
    inline
    void _releasePerIOData(IOCPMSG* io)
    {
        switch(io->msgType)
        {
        case MSG_IO_SEND:
            {
                if(!io->isUserSendBuff)
                    _releaseSendBuff(io->buff);
            }
            break;
        case MSG_IO_ACCEPT:
            {
                if(io->socket != INVALID_SOCKET)
                    closeSocket(io->socket);
                _releaseAcceptExBuff(io->acceptBuff);
            }
            break;
        case MSG_IO_RECEIVE:
            {
                _releaseReceivBuff(io->buff);
            }
            break;
        default:
            break;
        }
        m_heaps.releasePerIOData(io);
    }
    inline
    PERHANDLEDATA* _allocPerHandleData()
    {
        PERHANDLEDATA* ph = m_heaps.allocPerHandleData();
        ph->id = interlockedInc(&m_nextHadndleId);
        return ph;
    }
    inline
    void _releasePerHandleData(PERHANDLEDATA* perHandleData)
    {
        ASSERT(perHandleData->pendingios == 0);
        perHandleData->isAlive = false;
        m_heaps.releasePerHandleData(perHandleData);
    }

    // used by 64k handle manager
    void release64kObj(_64kObject* obj) OVERRIDE
    {
        if (obj)
        {
            PERHANDLEDATA* perHandler = static_cast<PERHANDLEDATA*>(obj);
            if (perHandler->netSocket != INVALID_SOCKET)
                closeSocket(perHandler->netSocket);

            _releasePerHandleData(perHandler);
        }
    }

private:
    //warning : completionKey must be safely to access
    void postRecvIO(PERHANDLEDATA* completionKey, IOCPMSG* perIOReuse = nullptr)
    {
        IOCPMSG* perIO = perIOReuse;

        if (perIO == nullptr)
        {
            ASSERT(m_64kHandle.is64kHandleValid(completionKey->selfRef));

            perIO = _allocPerIOData();
            if (perIO)
            {
                perIO->buff = _allocReceivBuff();
                perIO->receivBuffStart = perIO->buff;
                perIO->buffLen = RECEIVBUFF_LEN;
                perIO->receivBuffLen = perIO->buffLen;
                perIO->msgType = MSG_IO_RECEIVE;
            }

            if (!IOProceesStart(completionKey))
            {
                _releasePerIOData(perIO);
                return;
            }

            NLOG_INFO("{[%u]} reciev io[%u] start", completionKey->id, perIO->id);
            //debug
            completionKey->isRStarted = true;
        }
        //debug
        else
        {
            ASSERT(completionKey->pendingios >= 1);
        }

        SOCKET s = completionKey->netSocket;

        if (perIO == nullptr || perIO->buff == nullptr)
        {
            IOProcessEnd(completionKey, perIO);
            return;
        }

        WSABUF wsaBuff;
        wsaBuff.buf = perIO->receivBuffStart;
        wsaBuff.len = perIO->receivBuffLen;

        DWORD dwFlag = 0;

        memset(perIO, 0, sizeof(WSAOVERLAPPED));

        perIO->processed = 0;

        interlockedInc(&completionKey->prePostedReceiv);
        interlockedInc(&perIO->processed);

        int r = ::WSARecv(s, &wsaBuff, 1, NULL, &dwFlag, (LPWSAOVERLAPPED)perIO, NULL);
        auto er = WSAGetLastError();
        if (SOCKET_ERROR != r || WSAGetLastError() == WSA_IO_PENDING)
        {
            interlockedInc(&completionKey->postedReceiv);
            return;
        }

        //debug
        NLOG_INFO("{[%u]}: receiv io[%u] post failed: %p : %p [%d]", completionKey->id, perIO->id, completionKey, perIO, WSAGetLastError());
        long_t temp = interlockedDec(&perIO->processed);
        ASSERT(temp >= 0);

        completionKey->isRemoving = true;
        OnNetClose(0, completionKey);
        IOProcessEnd(completionKey, perIO);
        return;
    }

    //completionKey must be safely to access; 
    bool postAcceptIO(PERHANDLEDATA* completionKey, int postCount = ACCEPTEX_NUMPERROUND)
    {
        SOCKET s = completionKey->netSocket;
        IOCPMSG* perIO = nullptr;

        for (int i = 0; i < postCount; i++)
        {
            if (perIO == nullptr)
            {
                if (!IOProceesStart(completionKey))
                    return false;

                perIO = _allocPerIOData();
                if (perIO)
                {
                    perIO->msgType = MSG_IO_ACCEPT;
                    perIO->socket = INVALID_SOCKET;
                }
                else
                {
                    IOProcessEnd(completionKey, nullptr);
                    return false;
                }
            }

            SOCKET accept = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
            if (accept == INVALID_SOCKET)
            {
                IOProcessEnd(completionKey, perIO);
                return false;
            }

            perIO->socket = accept;

            char* acceptBuff = _allocAcceptExBuff();

            perIO->acceptBuff = acceptBuff;
            perIO->acceptBuffLen = ACCEPTEX_BUFFSIZE;

            interlockedInc(&perIO->processed);

            BOOL r = m_lpfAcceptEx(s, accept, acceptBuff, 0, sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, NULL, (LPWSAOVERLAPPED)perIO);

            if (r != TRUE && WSAGetLastError() != WSA_IO_PENDING)
            {
                IOProcessEnd(completionKey, perIO);
                return false;
            }

            interlockedInc(&completionKey->leftAcceptEx);

            //NLOG_INFO("IOCP: acceptex post failed: %p : %p",completionKey,perIO);

            perIO = nullptr;
        }

        return true;
    }
private:
    inline
    int OnReceiv(LPVOID dataBuff, int dataLen, PERHANDLEDATA* completionKey,bool canreserve = true)
    {
        return m_NetHandler->OnReceiv(dataBuff, dataLen, completionKey->handleContext, canreserve);
    }

    inline
    void OnSend(LPVOID dataBuff, DWORD needToSend, int sendLen, PERHANDLEDATA* completionKey)
    {}

    inline
    void OnNetClose(int closeReasson, PERHANDLEDATA* completionKey)
    {
        m_NetHandler->OnNetClose(closeReasson, completionKey->handleContext);
    }

    inline
    void OnNetConnected(int connectType, PERHANDLEDATA* completionKey)
    {
        m_NetHandler->OnNetConnected(connectType, completionKey->handleContext);
    }

    inline
    PERHANDLEDATA* OnAccepted(IOCPMSG* io, PERHANDLEDATA* completionKey)
    {
        long_t leftAcceptEx = interlockedDec(&completionKey->leftAcceptEx);
        if (leftAcceptEx < ACCEPTEX_NUMPERROUND >> 2)
        {
            int intend = ACCEPTEX_NUMPERROUND - (int)leftAcceptEx;
            postAcceptIO(completionKey, intend);
        }

        setsockopt(io->socket,
            SOL_SOCKET,
            SO_UPDATE_ACCEPT_CONTEXT,
            (char *)&completionKey->netSocket,
            sizeof(completionKey->netSocket));

        void* context = nullptr;

        SOCKADDR_IN localAddr; //= (SOCKADDR_IN*)io->acceptBuff;
        int temp = sizeof(localAddr);
        getsockname(io->socket, (sockaddr*)&localAddr, &temp);
        std::string localIP = inet_ntoa(localAddr.sin_addr);
        int localPort = ntohs(localAddr.sin_port);

        SOCKADDR_IN remoteAddr; //= (SOCKADDR_IN*)&io->acceptBuff[sizeof(SOCKADDR_IN)+16];
        temp = sizeof(remoteAddr);
        getpeername(io->socket, (sockaddr*)&remoteAddr, &temp);
        std::string remoteIP = inet_ntoa(remoteAddr.sin_addr);
        int remotePort = ntohs(remoteAddr.sin_port);

        // user must allocate thair resource for new net session. if refuse to accept,then this proc should return false
        if (!m_NetHandler->PrepareToAccept(localIP.c_str(), localPort, remoteIP.c_str(), remotePort, completionKey->handleContext, &context))
        {
            closeSocket(io->socket);
            io->socket = INVALID_SOCKET;
            return nullptr;
        }

        SOCKET s = io->socket;
        io->socket = INVALID_SOCKET;        //important!

        PERHANDLEDATA* newCompletionKey = _allocPerHandleData();
        if (!newCompletionKey)
        {
            m_NetHandler->AcceptFail(completionKey->handleContext, context);
            closeSocket(s);
            return nullptr;
        }

        newCompletionKey->handleContext = context;
        newCompletionKey->netSocket = s;

        H64K h = m_64kHandle.alloc64kHandle((_64kObject*)newCompletionKey);
        if (h == INVALID_64KHANDLE)
        {
            m_NetHandler->AcceptFail(completionKey->handleContext, context);
            closeSocket(s);
            _releasePerHandleData(newCompletionKey);
            return nullptr;
        }

        m_64kHandle.addRef64kHandle(h);
        newCompletionKey->selfRef = h;

        NLOG_INFO("accept : 绑定套接字[%x] 到 [%u] : %p", s, newCompletionKey->id, newCompletionKey);
        HANDLE hIOCP = ::CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)newCompletionKey, 0);

        if (hIOCP == NULL)
        {
            m_NetHandler->AcceptFail(completionKey->handleContext, context);
            m_64kHandle.decRef64kHandle(h);
        }

        m_64kHandle.addRef64kHandle(h);     //add reff for user
        m_NetHandler->OnNetAccepted(h, completionKey->handleContext, newCompletionKey->handleContext);

        return newCompletionKey;
    }

    inline
    bool IOProceesStart(PERHANDLEDATA* completionKey)
    {
        bool r = completionKey->AquireRundownProtection();
        if(r)
            interlockedInc(&completionKey->pendingios);
        return r;
    }

    inline
    void IOProcessEnd(PERHANDLEDATA* completionKey, IOCPMSG* io = nullptr,bool reuseIO = false)
    {
        auto pio = interlockedDec(&completionKey->pendingios);
        ASSERT(pio >= 0);

        if (io)
        {
            if (io->msgType == MSG_IO_RECEIVE)
            {
                NLOG_INFO("{[%u]} receiv io[%u] end", completionKey->id, io->id);
            }
        }

        if (completionKey->isRemoving)
        {
            if (io)
            {
                NLOG_INFO("{[%u]} io[%u] try to get rundown right", completionKey->id, io->id);
                _releasePerIOData(io);
                io = nullptr;
            }

            bool hasRunDownRight = completionKey->GetRundownRight();

            if (hasRunDownRight)
            {
                ASSERT(completionKey->pendingios == 0);

                //通知上层，可以安全释放资源
                // user must close the 64k handle at sometime or the resouce hold by 64k handle will keep not release
                m_NetHandler->OnNetDestroy(completionKey->handleContext);

                // release completionkey object
                m_64kHandle.decRef64kHandle(completionKey->selfRef);
            }

            return;
        }

        if (!reuseIO && io)
        {
            _releasePerIOData(io);
            io = nullptr;
        }

        completionKey->ReleaseRundownProtection();

        return;
    }

    bool ProcessCompletedIO(PERHANDLEDATA* completionKey, DWORD bytesTrans, IOCPMSG* io)
    {
        bool processed = true;
        int completeCode = io->msgType;

        switch (completeCode)
        {
            case MSG_IO_RECEIVE:
                {
                    completionKey->completedReceives[io->id % 20] = io;
                    interlockedInc(&completionKey->completedReceive);

                    if (bytesTrans <= 0)
                    {
                        completionKey->isRemoving = true;
                        OnNetClose(0, completionKey);
                        IOProcessEnd(completionKey, io);
                    }
                    else
                    {
                        int  consumedLen = 0;
                        int  leftLen = bytesTrans;

                        bool canreserve = (bytesTrans < io->receivBuffLen);

                        for (;;)
                        {
                            int c = OnReceiv(io->receivBuffStart, leftLen, completionKey, canreserve);
                            if (c < 0 || c == 0 && !canreserve)
                            {
                                io->receivBuffStart = io->buff;
                                io->receivBuffLen = io->buffLen;
                                break;
                            }

                            leftLen -= c;

                            if (leftLen <= 0)
                            {
                                io->receivBuffStart = io->buff;
                                io->receivBuffLen = io->buffLen;
                                break;
                            }

                            consumedLen += c;
                            io->receivBuffStart = &io->receivBuffStart[consumedLen];
                            io->receivBuffLen -= c;
                        }

                        ASSERT(io->receivBuffLen > 0);

                        if (io->receivBuffLen <= 0)
                        {
                            io->receivBuffStart = io->buff;
                            io->receivBuffLen = io->buffLen;
                        }

                        if (!completionKey->isRemoving)
                        {
                            postRecvIO(completionKey, io);
                        }
                        else
                        {
                            OnNetClose(0, completionKey);
                            IOProcessEnd(completionKey, io);
                        }
                    }
                }
            break;
            case MSG_IO_SEND:
            {
                completionKey->completedSends[io->id % 20] = io;
                ASSERT(io->msgType == MSG_IO_SEND);
                ASSERT(io->perHandle == completionKey);
                interlockedInc(&completionKey->completedSend);
                OnSend(io->buff, io->dataLen, bytesTrans, completionKey);
                IOProcessEnd(completionKey, io);
            }
            break;
            case MSG_IO_ACCEPT:
            {
                OnAccepted(io, completionKey);
                IOProcessEnd(completionKey, io);
            }
            break;
        default:
            processed = false;
            break;
        }

        return processed;
    }

    //-------------------------------
public:
    void closeNet(H64K h)
    {
        PERHANDLEDATA* perHandle = static_cast<PERHANDLEDATA*>(m_64kHandle.get64kObject(h));
        if (perHandle && perHandle->AquireRundownProtection())
        {
            perHandle->isRemoving = true;
            // shutdow to rescue!
            ::shutdown(perHandle->netSocket, SD_BOTH);
            perHandle->ReleaseRundownProtection();
        }
    }

    void closeHandle(H64K h, bool isCloseNet = false)
    {
        if (!isCloseNet)
        {
            m_64kHandle.decRef64kHandle(h);
            return;
        }

        closeNet(h);

        m_64kHandle.decRef64kHandle(h);
    }

    H64K connect(const char* szIP, const int port, void* lpCtx)
    {
        //build completionkey object
        PERHANDLEDATA* perHandleData = _allocPerHandleData();
        if (!perHandleData)
        {
            return INVALID_64KHANDLE;
        }

        H64K h = m_64kHandle.alloc64kHandle(perHandleData);
        if (h == INVALID_64KHANDLE)
        {
            _releasePerHandleData(perHandleData);
            return INVALID_64KHANDLE;
        }

        //build net socket;only TCP supported
        SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

        if (s == INVALID_SOCKET)
        {
            m_64kHandle.free64kHandle(h);
            _releasePerHandleData(perHandleData);
            return INVALID_64KHANDLE;
        }

        perHandleData->handleContext = lpCtx;
        perHandleData->netSocket = s;

        //ok, all resources be standby, inc reffernce to 64k handle
        m_64kHandle.addRef64kHandle(h);
        perHandleData->selfRef = h;

        NLOG_INFO("connect : 绑定套接字[%x] 到 [%d] : %p", s, perHandleData->id, perHandleData);

        // bind sokect and completionkey to completionport, prepare to accept async io
        HANDLE hIOCP = ::CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)perHandleData, 0);
        ASSERT(hIOCP != NULL);

        if (hIOCP == NULL)
        {
            // unlucky
            // release all, delegate releasing operation to 64k handle 
            m_64kHandle.decRef64kHandle(h);
            return INVALID_64KHANDLE;
        }

        SOCKADDR_IN saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = inet_addr(szIP);
        saddr.sin_port = htons(port);
        int r = 0;

        r = ::WSAConnect(s, (SOCKADDR*)&saddr, sizeof(saddr), NULL, NULL, NULL, NULL);
        if (SOCKET_ERROR != r)
        {
            // ok ,add additional reff to 64k handle for user and return that handle 
            m_64kHandle.addRef64kHandle(h);
            return h;
        }

        m_64kHandle.decRef64kHandle(h);
        return INVALID_64KHANDLE;
    }

    bool send(H64K iocpHandle, char* buff, int len, bool canLockBuff = false)
    {
        ASSERT(buff != nullptr);
        ASSERT(len > 0);

        if (buff == nullptr || len <= 0)
            return false;

        PERHANDLEDATA* perHandleData = static_cast<PERHANDLEDATA*> (m_64kHandle.get64kObject(iocpHandle));

        if (!perHandleData || !IOProceesStart(perHandleData))
            return false;

        //
        ASSERT(perHandleData->selfRef == iocpHandle);

        // allocate an per io object
        IOCPMSG* perIO = _allocPerIOData();
        if (perIO == nullptr)
        {
            IOProcessEnd(perHandleData, nullptr);
            return false;
        }

        char* sendBuff = buff;
        if (!canLockBuff)
        {
            sendBuff = _allocSendBuff(len);
            if (!sendBuff)
            {
                IOProcessEnd(perHandleData, perIO);
                return false;
            }
            memcpy(sendBuff, buff, len);
        }

        perIO->msgType = MSG_IO_SEND;
        perIO->buff = sendBuff;
        perIO->dataLen = len;
        perIO->isUserSendBuff = (sendBuff == buff);

        SOCKET s = perHandleData->netSocket;
        WSABUF wsaBuff;
        wsaBuff.buf = sendBuff;
        wsaBuff.len = len;

        //-------------------------------------------
        // for debug
        perHandleData->prePostedSends[perIO->id % 20] = perIO;
        perIO->id = interlockedInc(&perHandleData->prePostedSend);
        perIO->perHandle = perHandleData;
        interlockedInc(&perIO->processed);
        //-------------------------------------------

        //post async io to iocp
        int r = ::WSASend(s, &wsaBuff, 1, NULL, 0, (LPWSAOVERLAPPED)perIO, NULL);
        int er = WSAGetLastError();
        if (r != SOCKET_ERROR || er == WSA_IO_PENDING)
        {
            //debug
            interlockedInc(&perHandleData->postedSend);
            perHandleData->postedSends[perIO->id % 20] = perIO;

            return true;
        }

        //debug
        long_t temp = interlockedDec(&perIO->processed);
        assert(temp >= 0);
        NLOG_INFO(" {[%d]} failed sending [%u]; io:%p %p", perHandleData->id, perIO->id, perIO, perHandleData);

        perHandleData->isRemoving = true;

        // alert user
        OnNetClose(0, perHandleData);

        IOProcessEnd(perHandleData, perIO);
        return false;
    }

    void startReceiv(H64K iocpHandle, char* buff = nullptr, int len = 0)
    {
        PERHANDLEDATA* completionKey = static_cast<PERHANDLEDATA*>(m_64kHandle.get64kObject(iocpHandle));
        if (completionKey == nullptr)
            return;

        //debug
        completionKey->isRStarting = true;

        //TODO : 在 completionKey 中对网络状态做标志
        postRecvIO(completionKey);

        return;
    }

    H64K listen(int port, void* lpCtx, const char* szIP = nullptr)
    {
        SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

        if (s == INVALID_SOCKET)
            return INVALID_64KHANDLE;

        PERHANDLEDATA* perHandleData = _allocPerHandleData();
        if (!perHandleData)
        {
            ::closesocket(s);
            return INVALID_64KHANDLE;
        }

        H64K h = m_64kHandle.alloc64kHandle(perHandleData);
        if (h == INVALID_64KHANDLE)
        {
            _releasePerHandleData(perHandleData);
            ::closesocket(s);
            return h;
        }

        perHandleData->handleContext = lpCtx;
        perHandleData->netSocket = s;
        perHandleData->isListening = true;

        m_64kHandle.addRef64kHandle(h);
        perHandleData->selfRef = h;

        SOCKADDR_IN saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = szIP == nullptr ? htonl(INADDR_ANY) : inet_addr(szIP);
        saddr.sin_port = htons(port);

        int result = bind(s, (SOCKADDR *)&saddr, sizeof(saddr));
        if (result == SOCKET_ERROR || ::listen(s, SOMAXCONN) == SOCKET_ERROR)
        {
            m_64kHandle.decRef64kHandle(h);
            return INVALID_64KHANDLE;
        }

        HANDLE hIOCP = ::CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)perHandleData, 0);
        ASSERT(hIOCP != NULL);

        if (hIOCP == NULL || !postAcceptIO(perHandleData))
        {
            m_64kHandle.decRef64kHandle(h);
            return INVALID_64KHANDLE;
        }

        m_64kHandle.addRef64kHandle(h);
        return h;
    }

    long_t getId(H64K iocpHandle)
    {
        PERHANDLEDATA* perHandleData = static_cast<PERHANDLEDATA*> (m_64kHandle.get64kObject(iocpHandle));
        if (perHandleData)
            return perHandleData->id;
        else
            return -2;
    }

    int getRemoteAddr(H64K iocpHandle, std::string& remoteIP, int& remotePort)
    {
        PERHANDLEDATA* perHandleData = static_cast<PERHANDLEDATA*> (m_64kHandle.get64kObject(iocpHandle));
        if (!perHandleData)
            return SOCKET_ERROR;

        SOCKADDR_IN addr;
        int addrLen = sizeof(SOCKADDR_IN);
        int r = getpeername(perHandleData->netSocket, (sockaddr *)&addr, &addrLen);
        if (r == 0)
        {
            remoteIP = inet_ntoa(addr.sin_addr);
            remotePort = ntohs(addr.sin_port);
        }
        return r;
    }

    int getLocalAddr(H64K iocpHandle, std::string& localIP, int& localPort)
    {
        PERHANDLEDATA* perHandleData = static_cast<PERHANDLEDATA*> (m_64kHandle.get64kObject(iocpHandle));
        if (!perHandleData)
            return SOCKET_ERROR;

        SOCKADDR_IN addr;
        int addrLen = sizeof(SOCKADDR_IN);
        int r = getsockname(perHandleData->netSocket, (sockaddr *)&addr, &addrLen);
        if (r == 0)
        {
            localIP = inet_ntoa(addr.sin_addr);
            localPort = ntohs(addr.sin_port);
        }
        return r;
    }

private:
    static DWORD WINAPI IOCPWorkThreadProc(LPVOID lpParam)
    {
        static long_t ulStandbyThreads = 0;
        IOCPNet*  lpInstance = (IOCPNet*)lpParam;

        DWORD     dwNumOfBytes;
        PERHANDLEDATA* lpCompletionKey;
        IOCPMSG*  lpIocpMsg;

        bool isCuntinue = true;
        while (isCuntinue)
        {
            dwNumOfBytes = 0;
            lpCompletionKey = NULL;
            lpIocpMsg = NULL;

            interlockedInc(&ulStandbyThreads);
            BOOL isIOCPOK = ::GetQueuedCompletionStatus(
                lpInstance->m_hIOCP, &dwNumOfBytes,
                (ULONG_PTR*)&lpCompletionKey,
                (LPOVERLAPPED*)&lpIocpMsg, INFINITE);

            long_t standby = interlockedDec(&ulStandbyThreads);

            //if(standby <= 0)
            //    lpInstance->_startWorkTrhead(1);

            if (lpIocpMsg)
            {
                long_t temp = interlockedDec(&lpIocpMsg->processed);
                ASSERT(temp >= 0);
            }

            if (isIOCPOK)
            {
                if (!lpInstance->ProcessCompletedIO(lpCompletionKey, dwNumOfBytes, lpIocpMsg))
                {
                    if (lpIocpMsg->msgType == MSG_QUIT)
                    {
                        lpInstance->_releasePerIOData(lpIocpMsg);
                        break;
                    }
                }
            }
            else
            {
                if (lpIocpMsg != NULL)
                {
                    // a failed IO completed
                    DWORD dwError = ::GetLastError();

                    if (dwError == ERROR_OPERATION_ABORTED)
                    {
                        if (lpIocpMsg->msgType != MSG_IO_ACCEPT)
                        {
                            lpCompletionKey->isRemoving = true;
                        }
                    }
                    else
                    {
                        lpCompletionKey->isRemoving = true;
                    }

                    lpInstance->IOProcessEnd(lpCompletionKey, lpIocpMsg);
                    continue;
                }
                else
                {
                    NLOG_ERROR("IOCPNet : 严重错误：%d", GetLastError());
                    break;	//fatal error
                }
            }
        }

        return ::WSAGetLastError();
    }
};

#pragma warning(pop)

#endif //_IOCPNET_H_