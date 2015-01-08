#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <memory>
#include <list>

#include <windows.h>

namespace ThreadPool
{
    // 线程池 执行 单位 : eTask
    struct tKTask
    {
        virtual ~tKTask() = 0  {};
        virtual void process() = 0;
        virtual void except(std::exception_ptr&) = 0;

    protected:
        tKTask() {}
    private:
        tKTask(tKTask const &);
        tKTask& operator= (tKTask const&);
    };

    typedef std::shared_ptr<tKTask> KTASKSPTR;

    enum eMsgType
    {
        EMSG_WORKLOAD = 0,
        EMSG_POOLQUIT			//线程池退出
    };

    struct tKMsg
    {
        eMsgType msgType;
        KTASKSPTR workLoad;

        tKMsg(eMsgType msgType, KTASKSPTR workLoad)
            :msgType(msgType)
            , workLoad(workLoad)
        {}

        tKMsg(tKMsg const& proto)
            :msgType(proto.msgType)
            , workLoad(nullptr)
        {}

        tKMsg& operator= (tKMsg && rv)
        {
            msgType = rv.msgType;
            workLoad = std::move(rv.workLoad);
        }

        ~tKMsg()
        {}

    private:
        tKMsg& operator= (tKMsg const &);
    };

    struct tKMsgCumsummer
    {
        virtual bool processMsg(tKMsg*) = 0;
        virtual ~tKMsgCumsummer() = 0 {};
    };

    struct IThreadPool
    {
        virtual void quit() = 0;
        virtual void executeTask(KTASKSPTR task) = 0;
    };

    //-------------------------------
    // win
    static HANDLE hMMHeap;
    struct MemoryManager
    {
        static tKMsg* alloctMsg(eMsgType msgType, KTASKSPTR workLoad)
        {
            if (hMMHeap == NULL)
                hMMHeap = ::HeapCreate(0, 1024 * sizeof(tKMsg), 1024 * 1024 * sizeof(tKMsg));
            LPVOID mem = ::HeapAlloc(hMMHeap, 0, sizeof(tKMsg));

            return new(mem)tKMsg(msgType, workLoad);
        }

        static void freeMsg(tKMsg* msg)
        {
            //delete msg;
            msg->~tKMsg();
            ::HeapFree(hMMHeap, 0, msg);
        }
    };

}
#endif