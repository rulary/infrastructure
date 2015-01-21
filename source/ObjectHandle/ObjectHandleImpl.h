#ifndef _OBJECTHANDLE_IMPL_H_
#define _OBJECTHANDLE_IMPL_H_

#include "ObjectHandle.h"
#include "ThreadSync.h"
#include "interlocked.h"

using ThreadSync::ExLock;
using ThreadSync::Locker;

struct ObjectHandleTable
{
    uint32_t    handleTick;
    ExLock      handleLock;
    _IObjectManager* genericObjManager;
    _HandleTableMemAllocator*  memAllocator;

    static const unsigned int _MagicCode = 772476465;

    ObjectHandleTableEntry*  freeEntryList;
    ObjectHandleTableEntry*  handleTable;

    ObjectHandleTable()
        :genericObjManager(nullptr)
        , memAllocator(nullptr)
        , freeEntryList(nullptr)
        , handleTable(nullptr)
    {}

private:
    ObjectHandleTableEntry*  _lookupTableEntry(uint32_t handle, void* handleTable)
    {
        // clear directory bits
        ObjectHandle objecthandle;
        ObjectHandleTableEntry* pointerArray;

        objecthandle.handleValue = handle;
        pointerArray = (ObjectHandleTableEntry*)handleTable;

        int level = 0;

        if (objecthandle.level2Index)
            level = 2;
        else if (objecthandle.level1Index)
            level = 1;

        uint32_t pointerIndex = objecthandle.pointerIndex;

        if (level != 2)
        {
            pointerIndex = ObjectHandleIndex(pointerIndex);
            if (pointerIndex == 0)
                return nullptr;
        }

        switch (level)
        {
        case 2:
            handleTable = ((void **)handleTable)[objecthandle.level2Index];
            ASSERT(objecthandle.level1Index > 0);
        case 1:
            pointerArray = ((ObjectHandleTableEntry **)handleTable)[objecthandle.level1Index];
        case 0:
            return &pointerArray[pointerIndex];
            break;
        default:
            break;
        }

        return nullptr;
    }

    ObjectHandleTableEntry* _allocateObjectHandleEntry(ObjectHandleTable& handleTable)
    {
        Locker lock(&handleTable.handleLock);
        unsigned short htoIndex = 0;

        ObjectHandleTableEntry* freeEntry = handleTable.freeEntryList;

        if (!freeEntry)
            return nullptr;

        handleTable.freeEntryList = freeEntry->nextFreeEntry;

        return freeEntry;
    }

    bool _expandHandleTable()
    {
        return false;
    }

    uint32_t _allocObjectHandle(handleObject_t* objPtr, ObjectHandleTable& handleTable)
    {
        ObjectHandleTableEntry* freeEntry = _allocateObjectHandleEntry(handleTable);

        if (!freeEntry)
        {
            if (!_expandHandleTable())
                return INVALID_OBJECTHANDLE;

            freeEntry = _allocateObjectHandleEntry(handleTable);

            if (!freeEntry)
                return INVALID_OBJECTHANDLE;
            uint32_t handle = freeEntry->tableIndex;
            freeEntry->objectPointer = objPtr;

            initRundownProtectBlock(&freeEntry->rundownLock);

            return handle;
        }
    }
};



#endif