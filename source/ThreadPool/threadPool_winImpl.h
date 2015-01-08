
#define SEC *1000
#ifndef K
#	define K *1024
#endif

#ifndef M
#	define M *(1024 K)
#endif

class ThreadPool_WinImpl : public tThreadPoolImpl
{
public:
    virtual void init(int maxThread, tKMsgCumsummer* msgProcessor, bool autoBalance /* = true */)  override
    {
        if (bInited)
            return;
        bInited = true;
        assert(msgProcessor != nullptr);
        this->kMsgProcessor = msgProcessor;

        SYSTEM_INFO sysinfo = { 0 };
        ::GetNativeSystemInfo(&sysinfo);

        ULONG workThreadNum = max((DWORD)maxThread, sysinfo.dwNumberOfProcessors + 2);

        for (DWORD i = 0; i < workThreadNum; i++)
        {
            HANDLE h = ::CreateThread(NULL, 0, WorkThreadProc, this, 0, NULL);
            win_threads.insert(win_threads.end(), h);
        }
    }

    virtual bool queueMsg(tKMsg* msg)  override
    {
        return !bQuiting && _queueIOCPMsg(0, msg) == TRUE;
    }

    virtual void quit()  override
    {
        bQuiting = true;

        for (int i = 0; i < win_threads.size(); i++)
        {
            _queueIOCPMsg(-1, NULL);
        }

        std::vector<HANDLE>::iterator it;
        for (it = win_threads.begin(); it < win_threads.end(); it++)
        {
            ::WaitForSingleObject(*it, 20 SEC);
            ::CloseHandle(*it);
        }

        win_threads.clear();
    }

public:
    ThreadPool_WinImpl()
        :hCompletionPort(NULL)
        , hMsgHeap(NULL)
        , bQuiting(false)
        , bInited(false)
        , kMsgProcessor(nullptr)
    {
        _allocResource();
    }

    ThreadPool_WinImpl(int maxThread, tKMsgCumsummer* msgProcessor, bool autoBalance = true)
    {
        _allocResource();
        init(maxThread, msgProcessor, autoBalance);
    }

    ~ThreadPool_WinImpl()
    {
        quit();
        _releaseResource();
    }
private:
    HANDLE hCompletionPort;
    HANDLE hMsgHeap;

    bool bQuiting;
    bool bInited;

    tKMsgCumsummer* kMsgProcessor;		// not owned, do not release this pointer
    std::vector<HANDLE> win_threads;

    struct IOCPMSG
    {
        //OVERLAPPED 
        DWORD dwMsgType;
        LPVOID lpWorkLoad;
    };

    IOCPMSG* _allocateIOCPMsg()
    {
        void* mem = ::HeapAlloc(hMsgHeap, HEAP_ZERO_MEMORY, sizeof(IOCPMSG));
        //IOCPMSG* r = new (mem)IOCPMSG();
        return (IOCPMSG*)mem;
    }

    void _freeIOCPMsg(IOCPMSG* msg)
    {
        ::HeapFree(hMsgHeap, 0, msg);
    }

    BOOL _queueIOCPMsg(DWORD dwMsgType, LPVOID lpWorkLoad)
    {
        IOCPMSG* iocpMsg = _allocateIOCPMsg();
        iocpMsg->dwMsgType = dwMsgType;
        iocpMsg->lpWorkLoad = lpWorkLoad;
        return ::PostQueuedCompletionStatus(hCompletionPort, sizeof(tKMsg), (ULONG_PTR)this, (LPOVERLAPPED)iocpMsg);
    }

    void _allocResource()
    {
        hCompletionPort = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
        hMsgHeap = ::HeapCreate(0, 10 K * sizeof(IOCPMSG), 10 M * sizeof(IOCPMSG));
        win_threads.reserve(16);
    }

    void _releaseResource()
    {
        ::CloseHandle(hCompletionPort);
        ::HeapDestroy(hMsgHeap);
        //::CloseHandle(hMsgHeap);
        kMsgProcessor = nullptr;
    }
private:
    static DWORD WINAPI WorkThreadProc(LPVOID lpParam)
    {
        ThreadPool_WinImpl* pool = static_cast<ThreadPool_WinImpl*>(lpParam);

        DWORD dwNumOfBytes;
        ULONG_PTR lpCompletionKey;
        IOCPMSG * lpIocpMsg;

        bool isCuntinue = true;
        while (isCuntinue)
        {
            dwNumOfBytes = 0;
            lpCompletionKey = NULL;
            lpIocpMsg = NULL;

            BOOL isIOCPOK = ::GetQueuedCompletionStatus(pool->hCompletionPort, &dwNumOfBytes, &lpCompletionKey, (LPOVERLAPPED*)&lpIocpMsg, INFINITE);
            if (!isIOCPOK)
            {
                ;
            }
            else
            {
                if (lpIocpMsg == NULL)
                {
                    ;
                }
                else
                {
                    switch (lpIocpMsg->dwMsgType)
                    {
                    case 0:
                        {
                              tKMsg* kmsg = (tKMsg*)lpIocpMsg->lpWorkLoad;
                              pool->kMsgProcessor->processMsg(kmsg);
                        }
                        break;
                    case -1:
                        isCuntinue = false;
                        break;
                    default:
                        break;
                    }
                }
            }

            if (lpIocpMsg != NULL)
                pool->_freeIOCPMsg(lpIocpMsg);
        }
        return 0;
    }
};