#ifndef _OBJECTHANDLE_H_
#define _OBJECTHANDLE_H_

/*
  Object handle used by such objects that shared in multithreads \ asynchronus operation.
  Its purpose is to management object's life time automatically in thread-safe way.
  When using the handle mechanism, users can easily access object without take any other effords on managementing object's life time.

  Then idea is similar to reference count / shared_ptr, but with safe-access in anytime, even that the handle was closed and the object
  underground has been deleted.

  It's very convienient when programming with IOCP to mangement the Per-Handle Key objects while so many asynchronus IOs were referencing them.

  consults : Rundow Protection
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

typedef union _ObjectHandleTableCode
{
	struct
	{
#if defined (X64)
		uint64_t pointer : 62;
		uint64_t level : 2;
#else
		uint32_t pointer : 30;
		uint32_t level : 2;
#endif
	};

	void* rawPointerValue;
}ObjectHandleTableCode_t;

#define HandleFromIndex(t,i)				((t << OBJECT_HANDLE_PAGE_BIT_LEN) | i);				
#define ObjectHandleIndex(h)                (h & ~(OBJECT_HANDLE_DIRECTORY_SIZE - 1))
#define HANDLE_MAGIC_NUMBER					(772476465)

struct handleObject_t
{
    uint_t  magicCodeOfHandleObject;
    uint_t  refCount;

    virtual void  releaseObject(){};

protected:
    handleObject_t() {}
    ~handleObject_t() {}
};

enum  TableEntryFlag
{
	FREE = 0,
	INUSE = 1,
	RUNNINGDOWN = 2
};

typedef struct _ObjectHandleTableEntry
{
	int   flag;
	union
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
private:
    uint32_t    handleTick;
    ExLock      handleLock;

    _IObjectManager* genericObjManager;
    _HandleTableMemAllocator*  memAllocator;

    ObjectHandleTableEntry*  freeEntryList;
	ObjectHandleTableCode_t  handleTable;

	ObjectHandleTableEntry*  rundownedEntryList;

	int			tableLevelInit;
public:
	ObjectHandleTable(_IObjectManager* objManager, _HandleTableMemAllocator* allocator, int level = 0)
		: genericObjManager(objManager)
		, memAllocator(allocator)
        , freeEntryList(nullptr)
		, rundownedEntryList(nullptr)
		, tableLevelInit(level)
    {
		handleTable.rawPointerValue = nullptr;

		initTable();
	}

	void initTable()
	{
		handleTable.rawPointerValue = memAllocator->alloc(OBJECT_HANDLE_PAGE_SIZE * sizeof(ObjectHandleTableEntry));
		freeEntryList = (ObjectHandleTableEntry*)handleTable.rawPointerValue;

		ObjectHandleTableEntry* cur = freeEntryList;
		cur->flag = /*TableEntryFlag::*/FREE;
		cur->tableIndex = 0;
		ObjectHandleTableEntry* arrayOfHTE = freeEntryList;

		for (int i = 1; i < OBJECT_HANDLE_PAGE_SIZE; i++)
		{
			cur->nextFreeEntry = &arrayOfHTE[i];
			cur = cur->nextFreeEntry;
			cur->flag = /*TableEntryFlag::*/FREE;
			cur->tableIndex = i;
		}

		cur->nextFreeEntry = nullptr;

		handleTable.level = 0;
	}

private:
	bool _checkObjectSig(uint32_t handle, handleObject_t* object)
	{
		return  object->magicCodeOfHandleObject == (handle ^ HANDLE_MAGIC_NUMBER);
	}

	uint32_t _signObject(handleObject_t* object, uint32_t tableIndex)
	{
		uint32_t handle = HandleFromIndex(handleTick, tableIndex);
		object->magicCodeOfHandleObject = handle ^ HANDLE_MAGIC_NUMBER;
		return  handle;
	}

    ObjectHandleTableEntry*  _lookupTableEntry(uint32_t index)
    {
        ObjectHandle_t objecthandle;
        ObjectHandleTableEntry* pointerArray;

		objecthandle.handleValue = index;

		pointerArray = (ObjectHandleTableEntry*)handleTable.pointer;

        int level = 0;

		if (handleTable.level & 0x1)
            level = 1;
		else if (handleTable.level & 0x2)
            level = 2;

        uint32_t pointerIndex = objecthandle.pointerIndex;

        switch (level)
        {
        case 2:
			pointerArray = ((ObjectHandleTableEntry **)pointerArray)[objecthandle.level2Index];
			if (!pointerArray)
				break;
        case 1:
			pointerArray = ((ObjectHandleTableEntry **)pointerArray)[objecthandle.level1Index];
			if (!pointerArray)
				break;
        case 0:
            return &pointerArray[pointerIndex];
            break;
        default:
            break;
        }

        return nullptr;
    }

    ObjectHandleTableEntry* _allocateObjectHandleEntry()
    {
        unsigned short htoIndex = 0;

        ObjectHandleTableEntry* freeEntry = freeEntryList;

		if (!freeEntry)
            return nullptr;

		freeEntryList = freeEntryList->nextFreeEntry;

		freeEntry->flag = /*TableEntryFlag::*/INUSE;
        return freeEntry;
    }

	void _releaseObjectHandleEntry(ObjectHandleTableEntry* entry, int index)
	{
		//TODO: ASSERT & Ensure entry is in range
		entry->nextFreeEntry = freeEntryList;
		entry->tableIndex = index;
		entry->flag = /*TableEntryFlag::*/FREE;

		//TODO: fix it in non-level0 case
		ASSERT(&((ObjectHandleTableEntry*)handleTable.pointer)[index] == entry);

		freeEntryList = entry;
	}

    bool _expandHandleTable()
    {
        return false;
    }

	uint32_t _allocHandleForObject(handleObject_t* objPtr)
    {
		Locker lock(&handleLock);
        ObjectHandleTableEntry* freeEntry = _allocateObjectHandleEntry();

        if (!freeEntry)
        {
            if (!_expandHandleTable())
                return INVALID_OBJECTHANDLE;

            freeEntry = _allocateObjectHandleEntry();

            if (!freeEntry)
                return INVALID_OBJECTHANDLE;
        }

		uint32_t index = (uint32_t)freeEntry->tableIndex;

		// make a signature into object
		uint32_t handle = _signObject(objPtr, index);

		freeEntry->objectPointer = objPtr;

		initRundownProtectBlock(&freeEntry->rundownLock);

		//inc time tick
		handleTick++;

		return handle;
    }

	bool _freeObjectHandle(uint32_t handle)
	{
		Locker lock(&handleLock);

		ObjectHandleTableEntry* entry = _lookupTableEntry(ObjectHandleIndex(handle));

		if (!entry || entry->flag != 1)
			return false;

		if (!_checkObjectSig(handle, entry->objectPointer))
			return false;

		// check if we have rundown right of that object
		// Note : getRundownRight has a side effect that when applied, the object will be marked as running down.
		if (getRundownRight(&entry->rundownLock))
		{
			//terminate life cycle of object
			if (genericObjManager != nullptr)
			{
				genericObjManager->releaseObj(entry->objectPointer);
			}
			else
			{
				entry->objectPointer->releaseObject();
			}

			// free handle entry
			_releaseObjectHandleEntry(entry, ObjectHandleIndex(handle));
		}
		else
		{
			// insert that entry into 
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