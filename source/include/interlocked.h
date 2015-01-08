#ifndef _INTERLOCKED_H_
#define _INTERLOCKED_H_

#include "dataType.h"

#if defined(_WINDOWS_)

#define val_compare_and_swap32(destPtr, oldValue, newValue)        \
    InterlockedCompareExchange((LONG volatile *)(destPtr), \
    (LONG)(newValue), (LONG)(oldValue))

#define val_compare_and_swap64(destPtr, oldValue, newValue)        \
    InterlockedCompareExchange64((LONG64 volatile *)(destPtr), \
    (LONG64)(newValue), (LONG64)(oldValue))

#define bool_compare_and_swap32(destPtr, oldValue, newValue)       \
    (InterlockedCompareExchange((LONG volatile *)(destPtr), \
    (LONG)(newValue), (LONG)(oldValue))         \
    == (LONG)(oldValue))

#define bool_compare_and_swap64(destPtr, oldValue, newValue)       \
    (InterlockedCompareExchange64((LONG64 volatile *)(destPtr), \
    (LONG64)(newValue), (LONG64)(oldValue))     \
    == (LONG64)(oldValue))

#define lock_test_and_set32(destPtr, newValue)                     \
    InterlockedExchange((LONG volatile *)(destPtr), (LONG)(newValue))

#define fetch_and_add32(destPtr, addValue)                         \
    InterlockedExchangeAdd((LONG volatile *)(destPtr), (LONG)(addValue))

#define fetch_and_add64(destPtr, addValue)                         \
    InterlockedExchangeAdd64((LONGLONG volatile *)(destPtr), (LONGLONG)(addValue))

#define lock_inc32(destPtr)                                        \
    InterlockedIncrement((LONG volatile *)destPtr)

#define lock_dec32(destPtr)                                        \
    InterlockedDecrement((LONG volatile *)destPtr)

#define lock_inc64(destPtr)                                        \
    InterlockedIncrement64(destPtr)

#define lock_dec64(destPtr)                                        \
    InterlockedDecrement64(destPtr)



#elif defined(__GUNC__) || defined(__linux__) \
    || defined(__clang__) || defined(__APPLE__) || defined(__FreeBSD__) \
    || defined(__CYGWIN__) || defined(__MINGW32__)

#define val_compare_and_swap32(destPtr, oldValue, newValue)        \
    __sync_val_compare_and_swap((uint32_t volatile *)(destPtr), \
    (uint32_t)(oldValue), (uint32_t)(newValue))

#define val_compare_and_swap64(destPtr, oldValue, newValue)        \
    __sync_val_compare_and_swap((uint64_t volatile *)(destPtr), \
    (uint64_t)(oldValue), (uint64_t)(newValue))

#define val_compare_and_swap(destPtr, oldValue, newValue)          \
    __sync_val_compare_and_swap((destPtr), (oldValue), (newValue))

#define bool_compare_and_swap32(destPtr, oldValue, newValue)       \
    __sync_bool_compare_and_swap((uint32_t volatile *)(destPtr), \
    (uint32_t)(oldValue), (uint32_t)(newValue))

#define bool_compare_and_swap64(destPtr, oldValue, newValue)       \
    __sync_bool_compare_and_swap((uint64_t volatile *)(destPtr), \
    (uint64_t)(oldValue), (uint64_t)(newValue))

#define bool_compare_and_swap(destPtr, oldValue, newValue)         \
    __sync_bool_compare_and_swap((destPtr), (oldValue), (newValue))

#define lock_test_and_set32(destPtr, newValue)                     \
    __sync_lock_test_and_set((uint32_t volatile *)(destPtr), \
    (uint32_t)(newValue))

#define fetch_and_add32(destPtr, addValue)                         \
    __sync_fetch_and_add((int32_t volatile *)(destPtr), \
    (int32_t)(addValue))

#define fetch_and_add64(destPtr, addValue)                         \
    __sync_fetch_and_add((int64_t volatile *)(destPtr), \
    (int64_t)(addValue))

#define lock_inc32(destPtr)                                        \
    __sync_add_and_fetch((int32_t volatile *)(destPtr), \
    (int32_t)1)

#define lock_dec32(destPtr)                                        \
    __sync_sub_and_fetch((int32_t volatile *)(destPtr), \
    (int32_t)1)

#define lock_inc64(destPtr)                                        \
    __sync_add_and_fetch((int64_t volatile *)(destPtr), \
    (int64_t)1)

#define lock_dec64(destPtr)                                        \
    __sync_sub_and_fetch((int64_t volatile *)(destPtr), \
    (int64_t)1)

#endif

#ifdef X64

inline
long_t  interlockedInc(long_t volatile * dest)
{
    return lock_inc64(dest);
}

inline
long_t  interlockedDec(long_t volatile * dest)
{
    return lock_dec64(dest);
}

inline
long_t interlockedCmpChange(long_t volatile * dest, long_t value, long_t comparand)
{
    return val_compare_and_swap64(dest, comparand, value);
}

#else

inline
long_t  interlockedInc(long_t volatile * dest)
{
    return lock_inc32(dest);
}

inline
long_t  interlockedDec(long_t volatile * dest)
{
    return lock_dec32(dest);
}

inline
long_t interlockedCmpChange(long_t volatile * dest, long_t value, long_t comparand)
{
    return val_compare_and_swap32(dest, comparand, value);
}

#endif  // X64

#endif // _INTERLOCKED_H_