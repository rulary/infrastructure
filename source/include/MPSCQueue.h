
#ifndef _MDSCQUEUE_H_
#define _MDSCQUEUE_H_

#include "SerializedProcessor.h"
#include "interlocked.h"
#include "MonopolyHeap.h"
#include <assert.h>
#include <new>

#define _LOCKFREE_KEY_MASK             1
#define _LOCKFREE_GET_STATE(raw_ptr)   (raw_ptr | _LOCKFREE_KEY_MASK)
#define _LOCKFREE_GET_PTR(raw_key)     (raw_key & ((ulong_t)~_LOCKFREE_KEY_MASK))

#define MPSC_QUEUE_CONSUMER   1
#define MPSC_QUEUE_PRODUCER   0

//MPSC: multy producer-single consumer
// the lock free note: 
// producer will be lock free from the queue,but consumer will be NOT.
// if the consummer die holding the queue, then no other one can
// own the queue again, the queue become a black hold then.
class MPSCLockFreeQueue
{
    struct QueueEntry
    {
#ifdef DEBUG_ENABLED
        //for debug
        long_t Id;
        long_t proccessed;
#endif
        union
        {
            QueueEntry* pre;
            ulong_t state;
        };

        QueueEntry* next;
        void* value;

        QueueEntry()
        {
#ifdef DEBUG_ENABLED
            Id = -1;
            proccessed = 0;
#endif
            pre = this;
            next = nullptr;
            value = nullptr;
        }
    };

    QueueEntry      m_queue;
    MonoHeap*       m_entryHeap;

    MonoHeap*       m_externelHeap;
#ifdef DEBUG_ENABLED
    //for debug
    long_t  inserted;
    long_t  removed;
#endif

public:

#ifdef DEBUG_ENABLED
    long_t getRemoved() { return removed; }
    long_t getInserted() { return inserted; }
#endif

    int getPendingCount()
    {
        int c = 0;
        auto next = (QueueEntry *)_LOCKFREE_GET_PTR(m_queue.state);
        for (;; c++)
        {
            if (next == &m_queue)
                break;

            next = next->pre;
        }
        return c;
    }

public:
    MPSCLockFreeQueue(MonoHeap* externelHeap = nullptr)
        :m_entryHeap(externelHeap)
        , m_externelHeap(externelHeap)
    {
        if (!m_entryHeap)
            m_entryHeap = new MonoHeap(sizeof(QueueEntry));
#ifdef DEBUG_ENABLED
        inserted = 0;
        removed = 0;
#endif
    }

    ~MPSCLockFreeQueue()
    {
        if (!m_externelHeap)
            delete m_entryHeap;
    }

private:
    QueueEntry* _allocTFListElement()
    {
        static long_t entryId = 0;
        void* mem = m_entryHeap->alloc();
        auto ptr = new(mem)QueueEntry();

#ifdef DEBUG_ENABLED
        auto id = interlockedInc(&entryId);
        ptr->Id = id;
#endif
        return ptr;
    }

    void _freeTFListElement(QueueEntry* ptr)
    {
#ifdef DEBUG_ENABLED
        return;
#endif
        m_entryHeap->free(ptr);
    }

    // Enqueue an entry of type QueueEntry into queue and try to get the ownership
    // If haved the ownership,return value will be 1,
    // otherwise will return 0
    // NOTE : the entry whould hanve been enqueued when successfully return value ge to zero 
    //        otherwise this function will keep spining forever or throw exception
    int _enqueueAndTryOwnRight(QueueEntry* newEntry)
    {
        ulong_t headPtr = (ulong_t)&m_queue;
        ulong_t ptr = (ulong_t)newEntry;
        ulong_t newstate = _LOCKFREE_GET_STATE(ptr);

        ulong_t state = m_queue.state;
        newEntry->next = nullptr;

        for (;;)
        {
            newEntry->pre = (QueueEntry*)_LOCKFREE_GET_PTR(state);

            //try insert into list
            ptr = interlockedCmpChange((long_t*)&m_queue.state, newstate, state);
            if (ptr != state)
            {
                state = ptr;
                continue;
            }
#ifdef DEBUG_ENABLED
            //entry enqueued 
            interlockedInc(&inserted);
#endif

            if ((ptr & _LOCKFREE_KEY_MASK) == 0)  // got ownership of the queue
            {
                return MPSC_QUEUE_CONSUMER;
            }
            else
            {
                //NOTE: it is NOT safe to access newEntry¡¢queue tail from here without owning the queue
                return MPSC_QUEUE_PRODUCER;   // just inserted
            }
        }

        assert(false);

        return -1;
    }

    // Dequeue entry in FIFO order 
    // This function will release ownership state when nessesary
    // Ensure that all entry value returned by this function be consummed when this function renturn nullptr,
    // i.e the caller must NOT cache/deffer the consumming of entry value that returned by this function
    // code example :
    //    for(;;){
    //        auto value = _getNextFragment();
    //        if(value)
    //             ;//do process immediately
    //        else
    //             break;
    //    }
    // NOTE: Only the one who are currently owning the queue should call this function
    //       i.e. this is a multy producer--one consummer type queue
    //       what are interisting here is that the only one consummer can be any one,
    //       any one can try to own the list
    void* _dequeue()
    {
        ulong_t headPtr = (ulong_t)&m_queue;
        ulong_t headState = _LOCKFREE_GET_STATE(headPtr);
        ulong_t prePre = 0;
        void* result = nullptr;

        //dequeue an entry from queue
        for (;;)    //it is NOT a loop here
        {
            ulong_t state = m_queue.state;
            if (state == headState)
            {
                // the queue maybe empty now,try to release the ownership state
                state = interlockedCmpChange((long_t*)&m_queue.state, headPtr, headState);

                if (state == headState)
                {
                    // ownership released,all work down
                    result = nullptr;
                    break;
                }
            }

            // it is safe to access an element here while we still owning the queue
            QueueEntry* entry = (QueueEntry*)_LOCKFREE_GET_PTR(state);
            ulong_t temp = state;

            // if there is only one entry
            if (entry->pre == &m_queue || entry->pre->pre == nullptr)
            {
                // try to dequeue it
                temp = interlockedCmpChange((long_t*)&m_queue.state, headState, state);
                if (temp == state)
                {
                    if (entry->pre != &m_queue)
                    {
                        if (entry->pre->pre == nullptr)
                        {
                            _freeTFListElement(entry->pre);
                        }
                    }
                    result = entry->value;

#ifdef DEBUG_ENABLED
                    entry->proccessed++;
                    assert(entry->proccessed == 1);
#endif
                    _freeTFListElement(entry);

                    m_queue.next = nullptr;  // important

#ifdef DEBUG_ENABLED
                    //interlockedInc(&removed);
                    removed++;
#endif
                    break;
                }

                entry = (QueueEntry*)_LOCKFREE_GET_PTR(temp);
            }

            // if we got here it means that there are two or more entries in the queue,just consume one
            assert(entry->pre != &m_queue);

            // find the tail entry
            if (!m_queue.next)
                _linkReverse();

            // dequeue tail element
            entry = m_queue.next;
            result = entry->value;
            m_queue.next = entry->next;

#ifdef DEBUG_ENABLED
            entry->proccessed++;
            assert(entry->proccessed == 1);
#endif
            if (m_queue.next)
            {
                m_queue.next->pre = &m_queue;
                _freeTFListElement(entry);
            }
            else
            {
                entry->pre = nullptr;
                assert(m_queue.state != _LOCKFREE_GET_STATE((long_t)entry));
            }

#ifdef DEBUG_ENABLED
            removed++;
#endif
            break;
        }

        return result;
    }

    // called by queue owner
    // link the queue elements by reverse order
    // while consumming the queue,we should start at tail,but the reverse order link is not initialized in first place
    void _linkReverse()
    {
        QueueEntry* entry = (QueueEntry*)_LOCKFREE_GET_PTR(m_queue.state);

        assert(entry->pre != &m_queue);
        assert(entry->pre != nullptr);

        int i = 0;
        //NOTE: we skip the "first" element of queue because it should keep open for producer to enqueue new element
        while (entry != &m_queue)
        {
            i++;
            entry->pre->next = entry;
            entry = entry->pre;

            if (entry->pre == nullptr)
            {
                entry->next->pre = &m_queue;
                m_queue.next = entry->next;
                _freeTFListElement(entry);
                break;
            }
        }
    }

public:

    // important: only one that currently owning the queue should call this function !!!!
    inline
    void ReleaseOwnership()
    {
        ulong_t state = m_queue.state;

        for (;;)
        {
            ulong_t ptr = _LOCKFREE_GET_PTR(state);
            ulong_t v = interlockedCmpChange((long_t*)&m_queue.state, ptr, state);
            if (v == state)
                break;

            state = v;
        }
    }

    // queue a task fragment into list and try to get ownership of that list
    // queue operation will allways be success orelse will throw an exception
    // if got ownership,renturn true otherwise false
    inline
    int EnQueue(void* value)
    {
        QueueEntry* lfqe = _allocTFListElement();
        assert(lfqe != nullptr);

        lfqe->value = value;

        return _enqueueAndTryOwnRight(lfqe);
    }

    // important: only one that currently owning the queue should call this function !!!!
    inline
    void* Dequeue()
    {
        return _dequeue();
    }

    // for show
    // process all task fragments in the list untill successfully exit ownership state
    int Process(int maxProcessTaskFragmentNum = -1)
    {
        if (maxProcessTaskFragmentNum == 0)
        {
            ReleaseOwnership();
            return 0;
        }

        int processed = 0;
        for (int i = 0;; i++)
        {
            ITaskFragment* next = (ITaskFragment*)Dequeue();
            if (!next)
                break;
            //Sleep(0);
            // can safly process task fragment here
            next->Process();
            processed++;

            if (maxProcessTaskFragmentNum > 0 && processed >= maxProcessTaskFragmentNum)
            {
                ReleaseOwnership();
                break;
            }
        }

        return processed;
    }
};
#endif