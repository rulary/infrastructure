#ifndef _64KHANDLETYPE_H_
#define _64KHANDLETYPE_H_

#include <stdint.h>

#define H64K                uint32_t
#define INVALID_64KHANDLE  ((H64K)-1)

struct _64kObject
{
    uint_t  magicCodeOfHandleObject;
    virtual void  releaseObject(){};

protected:
    _64kObject() {}
    ~_64kObject() {}
};

struct I64kObjectManager
{
    virtual void  release64kObj(_64kObject* obj) = 0;
};

#endif