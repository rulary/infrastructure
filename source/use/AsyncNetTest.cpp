#include "AsyncNet.h"
#include "MPSCQueue.h"
#include "mem_dump.h"

#include <vector>
#include <conio.h>
#include <thread>

#define MEMDBSERVER_IP    "127.0.0.1"
#define MAINMEMSERVERPOT  8600

long_t totalStart = 0;
long_t totalStoped = 0;
bool   isStopAsyncNet = false;
using namespace AsyncNeting;

class TestNetSession 
    : public INetSession_SYS
    , private INetSession
{
    NETHANDLE m_NetHandle;
    AsyncNet* m_NetManager;
    long_t    m_sId;

    string m_LocalIP;
    int    m_LocalPort;

    string m_RemoteIP;
    int    m_RemotePort;

    MPSCLockFreeQueue m_sProcessor;    //用于单线程化执行

public:
    TestNetSession(MonoHeap* processHeap = nullptr)
        :m_LocalIP("")
        , m_LocalPort(0)
        , m_RemoteIP("")
        , m_RemotePort(0)
        , m_sId(-1)
        , m_sProcessor(processHeap)
    {}

public:
    //----------------------------
    //INetSession

    bool send(char* buff, int len) override
    {
        //printf("网络[%u] 发送数据长度： %d  \r\n",m_sId,len);
        return m_NetManager->send(m_NetHandle, buff, len);
    }

    void stopNet() override
    {
        m_NetManager->closeNetHandle(m_NetHandle);
    }

public:
    INetSession* getNetSessionInterface() override
    {
        return (INetSession*)this;
    }

    void Start(NETHANDLE netHandle,
        AsyncNet* netManager,
        const char* szLocalIP,
        const int iLocalPort,
        const char* szRemoteIP,
        const int iRemotePort,
        long_t netSessionId) override
    {
        m_NetHandle = netHandle;
        m_NetManager = netManager;

        m_sId = netSessionId;
        m_LocalIP = szLocalIP;
        m_LocalPort = iLocalPort;
        m_RemoteIP = szRemoteIP;
        m_RemotePort = iRemotePort;

        //printf("网络[%u]：%d => %d 会话已经开始  \r\n",m_sId,iLocalPort,iRemotePort);
        char buff[64];
        //= "wewqewewqewqrwrewrweffgggrtfgertewtewtfegtttttttttttttttttttwwwwwwwwwwwwwwwtewwewwr";
        //send(buff, sizeof(buff));
        /*
        int c = 1000;
        while(c-- > 0)
        {
        if(!Send("wewqewewqewqrwrewrwe",15))
        break;
        }*/
    }

    void Stop() override
    {
        interlockedInc(&totalStoped);
        printf("释放网络[%u] %d => %d 会话资源 <%u>\r\n", m_sId, m_LocalPort, m_RemotePort, totalStoped);
        m_NetManager->closeNetHandle(m_NetHandle);
    }

public:
    void OnNetOpen() override
    {
        //printf("网络：%s : %d 已经打开  \r\n",m_IP.c_str(),m_Port);
    }

    int  OnReceiv(char* buff, int dataLen, bool canReserve) override
    {
        //printf("网络[%u]：%d => %d 收到数据：长度为:%d  \r\n", m_sId, m_LocalPort, m_RemotePort, dataLen);
        if (m_sProcessor.EnQueue(this) == 1)
        {
            void* next = m_sProcessor.Dequeue();
            do{
                send(buff, dataLen);
                next = m_sProcessor.Dequeue();
            } while (next);
        }
        return dataLen;
    }

    void OnNetClose() override
    {
        //printf("网络：%d => %d 已经关闭  \r\n",m_LocalPort,m_RemotePort);
    }
};

template<typename S>
class SessionFactory : public INetSessionFactory
{
    MonoHeap* m_processHeap;
public:
    SessionFactory()
    {
        m_processHeap = new MonoHeap(64);
    }

    ~SessionFactory()
    {}

    INetSession_SYS* CreateSession() override
    {
        return new S(m_processHeap);
    }

    void DestroySession(INetSession_SYS* session) override
    {
        delete session;
    }
};

static
std::shared_ptr<std::thread> _testThread;

void StopAsyncNet()
{
    isStopAsyncNet = true;
    _testThread->join();
}

void AsyncTestThread()
{
    
    AsyncNet iocpNetTest;
    const char* sessionName = "test";
    iocpNetTest.registerNetSession(sessionName, make_shared<SessionFactory<TestNetSession>>());

    bool r = iocpNetTest.listen(sessionName, MAINMEMSERVERPOT);

    if (r)
    {
        printf("端口监听已经启动，按任意键开始测试\r\n");
    }
    else
    {
        printf("端口监听失败!\r\n");
        return;
    }

    //_getch();

    std::vector<INetSession*> sessions;
    /**/
    srand(GetTickCount());
    for (int i = 1, index = 0;; i++)
    {
        auto session = iocpNetTest.startSession(sessionName, MEMDBSERVER_IP, MAINMEMSERVERPOT, true);
        if (rand() % 13 == 0)
        {
            //printf("press any key top stop \r\n");
            //_getch();
            if (session)
                session->stopNet();
        }
        else
        {
            if (session)
                sessions.push_back(session);
        }

        if (i % 2000 == 0)
        {
            //iocpNetTest.showAudit();
            printf(".");
            //_getch();
        }

        if (isStopAsyncNet)
        {
            break;
        }
    }

    for (int i = 0; i < sessions.size(); i++)
    {
        sessions[i]->stopNet();
        sessions[i] = nullptr;
    }
}

void StartAsyncTest()
{
    _testThread = std::make_shared<std::thread>(AsyncTestThread);
}