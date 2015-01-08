#ifndef _NETHANDLER_H_
#define _NETHANDLER_H_

#include "64kHandleType.h"
struct INetHandler
{
    virtual bool PrepareToAccept(const char* localIp, int localPort, const char* remoteIp, int remotePort, void* listenContext, void** newContext) = 0;
    virtual void AcceptFail(void* listenContext, void* newContext) = 0;
    virtual void OnNetAccepted(H64K netHandle, void* listenContext, void* newContext) = 0;
    virtual int  OnReceiv(void* dataBuff, int dataLen, void* context, bool canreserved) = 0;
    virtual void OnNetClose(int closeReasson, void* context) = 0;
    virtual void OnNetConnected(int connectType, void* context) = 0;
    virtual void OnNetDestroy(void* context) = 0;
};

#endif