#ifndef __MONOPOLYHEAP_H_
#define __MONOPOLYHEAP_H_

#include <WinBase.h>

class MonoHeap
{
    HANDLE m_heap;
    size_t m_eSize;

public:
    void* alloc(bool zeroInit = false)
    {
        return HeapAlloc(m_heap, zeroInit ? HEAP_ZERO_MEMORY : 0, m_eSize);
    }

    void free(void* ptr)
    {
        HeapFree(m_heap, 0, ptr);
    }

    MonoHeap(size_t enlementSize, size_t minLen = 1024, size_t maxLen = 0, DWORD dwHeapOPtion = 0)
        :m_eSize(enlementSize)
        , m_heap(NULL)
    {
        m_heap = HeapCreate(dwHeapOPtion, minLen * enlementSize, maxLen * enlementSize);
    }

    ~MonoHeap()
    {
        HeapDestroy(m_heap);
        m_heap = NULL;
    }
};

#endif