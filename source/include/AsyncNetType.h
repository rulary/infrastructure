#ifndef _ASYNCNETTYPE_H_
#define _ASYNCNETTYPE_H_

#include "dataType.h"
#include "NetHandler.h"

#define NETHANDLE           H64K
#define INVALID_NETHANDLE   INVALID_64KHANDLE

//-------------------------------------------------------
// for net user

struct INetSession
{
    virtual bool send(char* buff, int len) = 0;
    virtual void stopNet() = 0;
};

namespace AsyncNeting
{
    class  AsyncNet;
}

// for net layer
struct INetSession_SYS
{
    virtual void Start(NETHANDLE netHandle, AsyncNeting::AsyncNet* netManager, const char* szLocalIP, const int iLocalPort, const char* szRemoteIP, const int iRemotePort, long_t netSessionId) = 0;
    virtual void Stop() = 0;

    virtual void OnNetOpen() = 0;
    virtual int  OnReceiv(char* buff, int dataLen, bool canReserve) = 0;
    virtual void OnNetClose() = 0;

    virtual INetSession* getNetSessionInterface() = 0;
    virtual ~INetSession_SYS() = 0 {}
};

struct INetSessionFactory
{
    virtual INetSession_SYS* CreateSession() = 0;
    virtual void DestroySession(INetSession_SYS*) = 0;
};

#endif