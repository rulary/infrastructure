
#ifndef _SERIALIZEDPROCESSOR_H_
#define _SERIALIZEDPROCESSOR_H_

struct ITaskFragment
{
    virtual void Process() = 0;
};

#endif