#ifndef _64KHANDLE_H_
#define _64KHANDLE_H_

#include "64kHandleType.h"

#include "ThreadSync.h"
#include "interlocked.h"

#include <stdint.h>
#include <memory>

using ThreadSync::ExLock;
using ThreadSync::Locker;

class _64kHandle
{
    _64kObject* handleToObject[64 * 1024];
    int         handleTick;
    ExLock      handleLock;
    I64kObjectManager* objManager;

    static const unsigned int _64kMagicCode = 772476465;

public:
    _64kHandle(I64kObjectManager* objManager_ = nullptr)
    {
        objManager = objManager_;
        memset(handleToObject, 0, sizeof(void*)* 64 * 1024);
    }

    ~_64kHandle()
    {}

    H64K alloc64kHandle(_64kObject* obj)
    {
        Locker lock(&handleLock);
        unsigned short htoIndex = 0;
        bool found = false;

        for (int i = 0; i < 64 * 1024; i++)
        {
            if (handleToObject[i] == nullptr)
            {
                htoIndex = (unsigned short)i;
                found = true;
                break;
            }
        }

        if (!found)
            return (H64K)-1;

        int tick = handleTick++;

        H64K r = ((tick & 0xffff) << 16) | htoIndex;

        obj->magicCodeOfHandleObject = r ^ _64kMagicCode;
        handleToObject[htoIndex] = obj;

        return r;
    }

    void free64kHandle(H64K handle)
    {
        Locker lock(&handleLock);
        unsigned short htoIndex = handle & 0xffff;
        _64kObject* obj = handleToObject[htoIndex];

        if (obj && (handle ^ obj->magicCodeOfHandleObject) == _64kMagicCode)
            handleToObject[htoIndex] = nullptr;
    }

    bool is64kHandleValid(H64K handle)
    {
        unsigned short htoIndex = handle & 0xffff;
        _64kObject* obj = handleToObject[htoIndex];

        return (obj && (handle ^ obj->magicCodeOfHandleObject) == _64kMagicCode);
    }

    _64kObject* get64kObject(H64K handle)
    {
        if (!is64kHandleValid(handle))
            return nullptr;

        return handleToObject[handle & 0xffff];
    }

    bool addRef64kHandle(H64K handle)
    {
        Locker lock(&handleLock);
        unsigned short htoIndex = handle & 0xffff;
        _64kObject* obj = handleToObject[htoIndex];

        if (obj && (handle ^ obj->magicCodeOfHandleObject) == _64kMagicCode)
        {
            obj->refCount++;
            return true;
        }

        return false;
    }

    void decRef64kHandle(H64K handle)
    {
        Locker lock(&handleLock);
        unsigned short htoIndex = handle & 0xffff;
        _64kObject* obj = handleToObject[htoIndex];

        if (obj && (handle ^ obj->magicCodeOfHandleObject) == _64kMagicCode)
        {
            obj->refCount--;
            if (obj->refCount <= 0)
            {
                handleToObject[htoIndex] = nullptr;
                if (objManager != nullptr)
                    objManager->release64kObj(obj);
                else
                    obj->releaseObject();
            }
        }
    }
};
#endif