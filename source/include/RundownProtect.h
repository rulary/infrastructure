#ifndef _RUNDOWNPROTECT_H_

#define RUNDOWN_SHIFT           1
#define RUNDOWN_MARK            RUNDOWN_SHIFT
#define RUNDOWN_COUNT_MARK      (~RUNDOWN_SHIFT)
#define RUNDOWN_COUNT_INC       (1 << RUNDOWN_SHIFT)

#define WAITABLE

#undef  WAITABLE

#include "dataType.h"
#include "interlocked.h"

#ifdef WAITABLE
#include <Windows.h>    //for event
#endif

struct RUNDOWN_WAITBLOCK
{
    long_t        count;
#ifdef WAITABLE
    HANDLE        event;
#endif
};

struct RUNDOWN_PROTECT
{
    union
    {
        long_t              count;
        RUNDOWN_WAITBLOCK*  wait;
    };
};

void initRundownProtectBlock(RUNDOWN_PROTECT* rpBlock)
{
    rpBlock->count = 0;
}

bool aquireRundownProtection(RUNDOWN_PROTECT* rpBlock)
{
    long_t count = rpBlock->count;
    for (;;)
    {
        if (RUNDOWN_MARK & count)
            break;

        long_t newCount = count + RUNDOWN_COUNT_INC;

        newCount = interlockedCmpChange(&rpBlock->count, newCount, count);

        if (newCount == count)
            return true;

        count = newCount;
    }

    return false;
}

void releaseRundownProtection(RUNDOWN_PROTECT* rpBlock)
{
#ifndef WAITABLE
    long_t count = rpBlock->count;
    long_t newCount = count - RUNDOWN_COUNT_INC;

    for (;;)
    {
        newCount = interlockedCmpChange(&rpBlock->count, newCount, count);

        if (newCount == count)
            break;

        count = newCount;
        newCount = count - RUNDOWN_COUNT_INC;
    }

#else
    long_t count = rpBlock->count & RUNDOWN_COUNT_MARK;
    long_t newCount = count - RUNDOWN_COUNT_INC;

    newCount = interlockedCmpChange(&rpBlock->count, newCount, count);
    if (newCount == count)
    {
        return;
    }

    count = rpBlock->count;
    for (;;)
    {
        if (RUNDOWN_MARK & count)
            break;

        newCount = count - RUNDOWN_COUNT_INC;

        newCount = interlockedCmpChange(&rpBlock->count, newCount, count);
        if (newCount == count)
            return;

        count = newCount;
    }

    RUNDOWN_WAITBLOCK* waitBlockPtr = (RUNDOWN_WAITBLOCK*)(count & RUNDOWN_COUNT_MARK);
    if (!waitBlockPtr)
        return;

    count = interlockedDec(&waitBlockPtr->count);

    if (count == 0)
        SetEvent(waitBlockPtr->event);
#endif
}

#ifndef WAITABLE
bool getRundownRight(RUNDOWN_PROTECT* rpBlock)
{
    long_t count = rpBlock->count;
    for (;;)
    {
        long_t newCount = (count - RUNDOWN_COUNT_INC) | RUNDOWN_MARK;

        long_t value = interlockedCmpChange(&rpBlock->count, newCount, count);
        if (value == count)
        {
            if (newCount == RUNDOWN_MARK)
                return true;
            else
                return false;
        }

        count = value;
    }

    return false;
}
#endif
#ifdef WAITABLE
bool waitforRundownProtectionRelease(RUNDOWN_PROTECT* rpBlock, bool skipSelf = false)
{
    long_t intented = skipSelf ? (0 + RUNDOWN_COUNT_INC) : 0;

    long_t value = interlockedCmpChange(&rpBlock->count, RUNDOWN_MARK, intented);

    if (value == 0 || value == intented)
        return true;

    if (value & RUNDOWN_MARK)
        return false;

    RUNDOWN_WAITBLOCK waitBlock;
    waitBlock.event = CreateEvent(NULL, false, false, NULL);

    long_t waitBlockPtr = ((long_t)&waitBlock) | RUNDOWN_MARK;

    value = rpBlock->count & RUNDOWN_COUNT_MARK;
    waitBlock.count = value >> RUNDOWN_SHIFT;

    long_t newValue = interlockedCmpChange(&rpBlock->count, waitBlockPtr, value);

    if (newValue != value)
    {
        value = rpBlock->count;
        for (;;)
        {
            if (RUNDOWN_MARK & value)
            {
                CloseHandle(waitBlock.event);
                return false;
            }

            waitBlock.count = value >> RUNDOWN_SHIFT;
            newValue = interlockedCmpChange(&rpBlock->count, waitBlockPtr, value);
            if (newValue == value)
                break;

            value = newValue;
        }
    }

    if (skipSelf)
        waitBlock.count--;

    if (waitBlock.count > 0)
        WaitForSingleObject(waitBlock.event, INFINITE);

    CloseHandle(waitBlock.event);
    rpBlock->count = RUNDOWN_MARK;
    return true;
}

#endif

#define _RUNDOWNPROTECT_H_
#endif