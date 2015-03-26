#ifndef _IOCPNETTYPE_H_
#define _IOCPNETTYPE_H_

#include <Winsock2.h>
#include "dataType.h"
#include "64kHandleType.h"

#include "RundownProtect.h"

#define MINISERVER

#ifdef MINISERVER
#   define MIN_INTEND_HANDLE       600
#   define RECEIVBUFF_LEN          4096
#   define ACCEPTEX_NUMPERROUND    600
#elif defined(MEDIUMSERVER)
#   define MIN_INTEND_HANDLE       1000
#   define RECEIVBUFF_LEN          4096
#   define ACCEPTEX_NUMPERROUND    1000
#else 
#   define MIN_INTEND_HANDLE       6000
#   define RECEIVBUFF_LEN          4096 * 2
#   define ACCEPTEX_NUMPERROUND    3000
#endif

#define ACCEPTEX_BUFFSIZE       ((sizeof(SOCKADDR_IN) + 16) * 2)

enum IOCPMSG_TYPE
{
    MSG_QUIT = -1,
    MSG_IO_SEND = 0,
    MSG_IO_RECEIVE,
    MSG_IO_ACCEPT
};

struct PERHANDLEDATA;

struct IOCPMSG : public WSAOVERLAPPED
{
    IOCPMSG_TYPE    msgType;
    long_t          processed;
    PERHANDLEDATA*  perHandle;
    long_t          id;
    union
    {
        struct
        {
            char*           buff;
            char*           receivBuffStart;
            union
            {
                DWORD       receivBuffLen;
                BOOL        isUserSendBuff;
            };
            union
            {
                DWORD        buffLen;
                DWORD        dataLen;
            };
        };

        struct
        {
            SOCKET           socket;
            char*            acceptBuff;
            DWORD            acceptBuffLen;
        };
    };
};

struct PERHANDLEDATA : public _64kObject
{
    H64K                selfRef;
    RUNDOWN_PROTECT     rpBlock;
    SOCKET              netSocket;
    LPVOID              handleContext;
    long_t              leftAcceptEx;
    BOOL                isRemoving;
    BOOL                isAlive;

    long_t              id;
    BOOL                isListening;

    //-----------------------------------------------------
    long_t              prePostedSend;
    IOCPMSG*            prePostedSends[20];
    long_t              postedSend;
    IOCPMSG*            postedSends[20];
    long_t              completedSend;
    IOCPMSG*            completedSends[20];

    long_t              prePostedReceiv;
    IOCPMSG*            prePostedReceivs[20];
    long_t              postedReceiv;
    IOCPMSG*            postedReceivs[20];
    long_t              completedReceive;
    IOCPMSG*            completedReceives[20];

    long_t              pendingios;
    bool                isRStarting;
    bool                isRStarted;
	bool                isAccepted;
	bool                isReleased;

    PERHANDLEDATA()
        :selfRef(INVALID_64KHANDLE)
        , netSocket(INVALID_SOCKET)
        , handleContext(NULL)
        , leftAcceptEx(0)
        , isRemoving(false)
        , isAlive(true)
        , id(-1)
        , isListening(false)
    {
        initRundownProtectBlock(&rpBlock);

        prePostedSend = 0;
        postedSend = 0;
        completedSend = 0;

        prePostedReceiv = 0;
        postedReceiv = 0;
        completedReceive = 0;

        pendingios = 0;
        isRStarting = false;
        isRStarted = false;
		isAccepted = false;
		isReleased = false;

        memset(prePostedSends, 0, sizeof(prePostedSends));
        memset(postedSends, 0, sizeof(postedSends));
        memset(completedSends, 0, sizeof(completedSends));
        memset(prePostedReceivs, 0, sizeof(prePostedReceivs));
        memset(postedReceivs, 0, sizeof(postedReceivs));
        memset(completedReceives, 0, sizeof(completedReceives));
    }

    inline
    bool AquireRundownProtection()
    {
        return aquireRundownProtection(&rpBlock);
     }

    inline
    void ReleaseRundownProtection()
    {
        releaseRundownProtection(&rpBlock);
    }

    inline
    bool GetRundownRight()
    {
        return getRundownRight(&rpBlock);
    }

    //--------------------------------------------------------------
    long_t getCurrentPendingProtection()
    {
        return (rpBlock.count & RUNDOWN_COUNT_MARK) >> RUNDOWN_SHIFT;
    }
};

#endif