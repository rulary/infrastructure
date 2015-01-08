
//#define DEBUG_ENABLED
//#include "dataType.h"
#include <Windows.h>
#include "MPSCQueue.h"

#include <stdio.h>
#include <new>


class SerializableTask
{
public:
    HANDLE hHeap;
    int count;
    long_t cpart;

    SerializableTask()
    {
        count = 0;
        cpart = 0;

        hHeap = HeapCreate(0, 1024 * 16 , 0);
    }

    ~SerializableTask()
    {
        HeapDestroy(hHeap);
    }

    void* alloc(size_t s)
    {
        return HeapAlloc(hHeap,HEAP_ZERO_MEMORY,s);
    }

    void free(void* ptr)
    {
        HeapFree(hHeap,0,ptr);
    }
};

SerializableTask g_task;
MPSCLockFreeQueue g_processor;

long_t test;

class TaskFragment : public ITaskFragment
{
public:
    SerializableTask* m_task;
    DWORD m_threadId;
    int   m_localId;

    TaskFragment(SerializableTask* task, DWORD threadId,int localId)
        :m_task(task)
        , m_threadId(threadId)
        , m_localId(localId)
    {}

public:
    void Process() override
    {
        m_task->count++;
        printf(" <%05d [%05u% : %05d]> ", m_task->count, m_threadId, m_localId);
        interlockedInc(&m_task->cpart);
        auto c = interlockedInc(&test);
        //Sleep(2);
        assert(c == 1);

        interlockedDec(&test);

        //delete this;
        g_task.free(this);
    }
};

bool g_StopTest = false;

DWORD WINAPI MultWinWorkThreads(LPVOID lpParam)
{
    DWORD dwThreadId = GetCurrentThreadId();
    int count = (int)lpParam;


    while (count-- > 0)
    {
        void* ptr = g_task.alloc(sizeof(TaskFragment));
        TaskFragment* fragment = ::new (ptr)TaskFragment(&g_task, dwThreadId, count);
        //fragment->Process();
        /**/
        //if (g_processor.QueueTaskFragment(fragment))
        if (MPSC_QUEUE_CONSUMER == g_processor.EnQueue(fragment))
        {
            //Sleep(1);
            int c = g_processor.Process(-1);
            //if (c > 1)
            //    printf("c = %d \r\n", c);
        }
        /**/
        
        Sleep(2);
    }

    return 0;
}

void StartSPTest(int threadCount)
{
    g_StopTest = false;
    for (int i = 1; i <= threadCount; i++)
    {
        CloseHandle(CreateThread(NULL, 0, MultWinWorkThreads, (LPVOID)i, 0, NULL));
    }
}

void StopSPTest()
{
    g_StopTest = true;
}

void ShowCount()
{
#ifdef DEBUG_ENABLED
    printf("execed = %u pending = %d inqueue = %d dequeue: %d \r\n", 
        g_task.count, 
        g_processor.getPendingCount(), 
        g_processor.getInserted(), 
        g_processor.getRemoved());
#else
    printf("execed = %u pending = %d \r\n", g_task.count, g_processor.getPendingCount());
#endif
}