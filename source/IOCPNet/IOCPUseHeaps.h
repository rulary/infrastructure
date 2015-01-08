#ifndef _IOCPUERHEAPS_H_
#define _IOCPUERHEAPS_H_

#include "IOCPNetType.h"
#include <WinBase.h>

class HeapOfIOCP
{
    HANDLE hPerHandleHeap;
    HANDLE hPerIOMsgHeap;
    HANDLE hReceivBuffHeap;
    HANDLE hSendBuffHeap;
    HANDLE hAcceptExBuffHeap;

public:
    HeapOfIOCP()
        :hPerHandleHeap(NULL)
        , hPerIOMsgHeap(NULL)
        , hReceivBuffHeap(NULL)
        , hSendBuffHeap(NULL)
        , hAcceptExBuffHeap(NULL)
    {
        hPerHandleHeap = ::HeapCreate(0, sizeof(PERHANDLEDATA)* MIN_INTEND_HANDLE, 0);
        hPerIOMsgHeap = ::HeapCreate(0, sizeof(IOCPMSG)* 2 * MIN_INTEND_HANDLE, 0);
        hReceivBuffHeap = ::HeapCreate(0, RECEIVBUFF_LEN * MIN_INTEND_HANDLE, 0);
        hSendBuffHeap = ::HeapCreate(0, RECEIVBUFF_LEN * MIN_INTEND_HANDLE, 0);
        hAcceptExBuffHeap = ::HeapCreate(0, ACCEPTEX_BUFFSIZE * ACCEPTEX_NUMPERROUND, 0);
    }

    ~HeapOfIOCP()
    {
        ::HeapDestroy(hPerHandleHeap);
        ::HeapDestroy(hPerIOMsgHeap);
        ::HeapDestroy(hReceivBuffHeap);
        ::HeapDestroy(hSendBuffHeap);
        ::HeapDestroy(hAcceptExBuffHeap);
    }

public:
    inline
    char* allocReceivBuff()
    {
        return (char*)::HeapAlloc(hReceivBuffHeap, 0, RECEIVBUFF_LEN);
    }
    inline
    void releaseReceivBuff(char* buff)
    {
        ::HeapFree(hReceivBuffHeap, 0, buff);
    }
    inline
    char* allocSendBuff(int len)
    {
        return (char*)::HeapAlloc(hSendBuffHeap, 0, len);
    }
    inline
    void releaseSendBuff(char* buff)
    {
        ::HeapFree(hSendBuffHeap,0,buff);
    }
    inline
    char* allocAcceptExBuff()
    {
        return (char*)::HeapAlloc(hAcceptExBuffHeap, 0, ACCEPTEX_BUFFSIZE);
    }
    inline
    void releaseAcceptExBuff(char* buff)
    {
        ::HeapFree(hAcceptExBuffHeap,0,buff);
    }
    inline
    IOCPMSG* allocPerIOData()
    {
        void* newIO = ::HeapAlloc(hPerIOMsgHeap, HEAP_ZERO_MEMORY, sizeof(IOCPMSG));
        return new(newIO) IOCPMSG();
    }
    inline
    void releasePerIOData(IOCPMSG* io)
    {
         ::HeapFree(hPerIOMsgHeap, 0, io);
    }
    inline
    PERHANDLEDATA* allocPerHandleData()
    {

        void* mem = ::HeapAlloc(hPerHandleHeap, HEAP_ZERO_MEMORY, sizeof(PERHANDLEDATA));
        return new(mem) PERHANDLEDATA();
    }
    inline
    void releasePerHandleData(PERHANDLEDATA* perHandleData)
    {
        ::HeapFree(hPerHandleHeap, 0, perHandleData);
    }
};

#endif