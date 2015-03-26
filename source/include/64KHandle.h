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
    typedef union _64kHandleTableEntry_
    {
        struct
        {
            _64kHandleTableEntry_* nextFreeEntry;
            unsigned short         tableIndex;
        };
        struct
        {
            _64kObject* value;
            uint_t      refCount;
        };
        
    } _64kHandleTableEntry;

    static const int TABLE_SIZE = 64 * 1024;
    _64kHandleTableEntry handleToObject[TABLE_SIZE];
    _64kHandleTableEntry* freeEntryList;
    int         handleTick;
    ExLock      handleLock;
    I64kObjectManager* objManager;

    static const unsigned int _64kMagicCode = 772476465;

public:
    _64kHandle(I64kObjectManager* objManager_ = nullptr)
        :handleTick(0)
        , freeEntryList(nullptr)
        , objManager(objManager_)
    {
        memset(handleToObject, 0, sizeof(handleToObject[0]) * TABLE_SIZE);

        for (int i = 0; i < (TABLE_SIZE - 1); i++)
        {
            handleToObject[i].tableIndex = i;
            handleToObject[i].nextFreeEntry = &handleToObject[i + 1];
        }
        handleToObject[TABLE_SIZE - 1].tableIndex = TABLE_SIZE - 1;
        handleToObject[TABLE_SIZE - 1].nextFreeEntry = nullptr;
        freeEntryList = handleToObject;
    }

    ~_64kHandle()
    {
        ;
    }

    H64K alloc64kHandle(_64kObject* obj)
    {
        Locker lock(&handleLock);
        
        if (freeEntryList == nullptr)
            return (H64K)-1;

        auto freeEntry = freeEntryList;
        freeEntryList = freeEntryList->nextFreeEntry;

        unsigned short htoIndex = freeEntry->tableIndex;

        int tick = handleTick++;

        if (handleTick >= TABLE_SIZE - 1)
            handleTick++;

        H64K r = ((tick & 0xffff) << 16) | htoIndex;

        obj->magicCodeOfHandleObject = r ^ _64kMagicCode;
        freeEntry->value = obj;
        freeEntry->refCount = 0;

        return r;
    }

    void free64kHandle(H64K handle)
    {
        Locker lock(&handleLock);
        unsigned short htoIndex = handle & 0xffff;

        auto &entry = handleToObject[htoIndex];
        _64kObject* obj = entry.value;

        if (obj && (handle ^ obj->magicCodeOfHandleObject) == _64kMagicCode)
        {
            entry.value = nullptr;
            entry.refCount = 0;

            entry.nextFreeEntry = freeEntryList;
            entry.tableIndex = htoIndex;
            freeEntryList = &entry;
        } 
    }

    bool is64kHandleValid(H64K handle)
    {
        unsigned short htoIndex = handle & 0xffff;
        _64kObject* obj = handleToObject[htoIndex].value;

        return (obj && (handle ^ obj->magicCodeOfHandleObject) == _64kMagicCode);
    }

    _64kObject* get64kObject(H64K handle)
    {
        if (!is64kHandleValid(handle))
            return nullptr;

        return handleToObject[handle & 0xffff].value;
    }

    bool addRef64kHandle(H64K handle)
    {
        Locker lock(&handleLock);
        unsigned short htoIndex = handle & 0xffff;

        auto &entry = handleToObject[htoIndex];
        _64kObject* obj = entry.value;

        if (obj && (handle ^ obj->magicCodeOfHandleObject) == _64kMagicCode)
        {
            entry.refCount++;
            return true;
        }

        return false;
    }

    void decRef64kHandle(H64K handle)
    {
		if (handle == INVALID_64KHANDLE)
			return;

        Locker lock(&handleLock);
        unsigned short htoIndex = handle & 0xffff;

        auto &entry = handleToObject[htoIndex];
        _64kObject* obj = entry.value;

        if (obj && (handle ^ obj->magicCodeOfHandleObject) == _64kMagicCode)
        {
            entry.refCount--;
            if (entry.refCount <= 0)
            {
                entry.value = nullptr;
                entry.refCount = 0;

                entry.nextFreeEntry = freeEntryList;
                entry.tableIndex = htoIndex;
                freeEntryList = &entry;

                if (objManager != nullptr)
                    objManager->release64kObj(obj);
                else
                    obj->releaseObject();
            }
        }
    }
};
#endif