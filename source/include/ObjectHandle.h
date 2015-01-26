#ifndef _OBJECTHANDLE_H_
#define _OBJECTHANDLE_H_

/*
  Object handle used by such objects that shared in multithreads \ asynchronus operation.
  Its purpose is to management object life time automatically in thread-safe way.
  When use the handle mechanism, user can easily access object without take any other efford on object life time management.

  Idea is similar to reference count / shared_ptr, but with safe-access in anytime, even that the handle was closed and the object
  underground has been deleted.

  It's very convienient when programming with IOCP to mangement the Per-Handle key object while so many asynchronus IOs were refefencing it.

  consult : Rundow Protection
  */

#include "RundownProtect.h"
#include "ThreadSync.h"
#include "interlocked.h"
#include <cstdint>


#define OBJECT_HANDLE_PAGE_BIT_LEN          16
#define OBJECT_HANDLE_DIRECTORY_BIT_LEN     8

#define OBJECT_HANDLE_DIRECTORY_SIZE        (1 << OBJECT_HANDLE_DIRECTORY_BIT_LEN)
#define OBJECT_HANDLE_PAGE_SIZE             (1 << OBJECT_HANDLE_PAGE_BIT_LEN)

#define INVALID_OBJECTHANDLE                ((uint32_t)-1)

typedef union _ObjectHandle
{
    struct
    {
        uint32_t pointerIndex : OBJECT_HANDLE_PAGE_BIT_LEN;
        uint32_t level1Index : OBJECT_HANDLE_DIRECTORY_BIT_LEN;
        uint32_t level2Index : OBJECT_HANDLE_DIRECTORY_BIT_LEN;
    };

    uint32_t handleValue;
} ObjectHandle_t;

#define ObjectHandleIndex(h)                (h & ~(OBJECT_HANDLE_DIRECTORY_SIZE - 1))

struct handleObject_t
{
    uint_t  magicCodeOfHandleObject;
    uint_t  refCount;

    virtual void  releaseObject(){};

protected:
    handleObject_t() {}
    ~handleObject_t() {}
};

typedef union _ObjectHandleTableEntry
{
    struct
    {
        RUNDOWN_PROTECT     rundownLock;
        handleObject_t*     objectPointer;
    };

    struct
    {
        long_t                   tableIndex;
        _ObjectHandleTableEntry* nextFreeEntry;
    };
} ObjectHandleTableEntry;

struct _IObjectManager
{
    virtual void  releaseObj(handleObject_t* obj) = 0;

    virtual ~_IObjectManager() = 0 {}
};

struct _HandleTableMemAllocator
{
    virtual void*  alloc(size_t) = 0;
    virtual void   free(void*) = 0;

    virtual ~_HandleTableMemAllocator() = 0 {}
};


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
public:
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
        ObjectHandle_t objecthandle;
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


class ObjectHandle
{
    ObjectHandleTable& handleTable_;
public:
    handleObject_t* referenceObjectByHandle(uint32_t handle)
    {
        return nullptr;
    }

    bool referenceObjectByPointer(handleObject_t*)
    {
        return false;
    }
};

#endif