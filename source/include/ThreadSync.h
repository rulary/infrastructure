#ifndef _THREADSYNC_H_
#define _THREADSYNC_H_

#if !defined(_LINUX)
#   include <Windows.h>
#else
#endif

namespace ThreadSync
{
    struct ExLock;

    void EnterLock(ExLock* lock);
    void LeaveLock(ExLock* lock);

    struct Locker
    {
        ExLock* _lock;
        Locker(ExLock* lock) : _lock(lock){ EnterLock(lock); }
        ~Locker() { LeaveLock(_lock); }
    };

#if !defined(_LINUX)
    struct ExLock
    {
        CRITICAL_SECTION critical;
        ExLock()
        {
            InitializeCriticalSection(&critical);
        }
        ~ExLock()
        {
            DeleteCriticalSection(&critical);
        }
    };
    inline
    void EnterLock(ExLock* lock)
    {
        EnterCriticalSection(&lock->critical);
    }
    inline
    void LeaveLock(ExLock* lock)
    {
        LeaveCriticalSection(&lock->critical);
    }
#else
#endif
}

#endif