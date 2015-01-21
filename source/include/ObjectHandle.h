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

#include <cstdint>
#include "RundownProtect.h"

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
} ObjectHandle;

#define ObjectHandleIndex(h)                (h & ~(OBJECT_HANDLE_DIRECTORY_SIZE - 1))

struct handleObject_t
{
    unsigned int  magicCodeOfHandleObject;
    unsigned int  refCount;

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
    virtual void*  alloc(size_t ) = 0;
    virtual void   free(void*) = 0;

    virtual _HandleTableMemAllocator() = 0 {}
};

#include "../ObjectHandle/ObjectHandleImpl.h"
#endif