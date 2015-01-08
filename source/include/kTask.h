#ifndef _KTASK_H_
#define _KTASK_H_

#include "threadPool.h"
#include <functional>
#include <memory>
#include <queue>
//---------------------------------------------
namespace ThreadPool
{
    // 任务,可以直接在线程池中执行
    class Task : public tKTask
    {
    private:
        Task(Task const &);
        Task& operator=(Task const &);

    private:
        Task() : _taskContext(nullptr), _workload(), _onerror()
        {}

    public:
        Task(std::function<void(void*)> workload,
            void* context = nullptr,
            std::function<void(std::exception_ptr&, void*)> onerror = std::function<void(std::exception_ptr&, void*)>())
            :_taskContext(context)
            , _workload(workload)
            , _onerror(onerror)
        {}

    public:
        void process() override
        {
            _workload(_taskContext);
        }

        void except(std::exception_ptr& exptr) override
        {
            _onerror(exptr, _taskContext);
        }
    private:
        void* _taskContext;
        std::function<void(void*)> _workload;
        std::function<void(std::exception_ptr&, void*)> _onerror;
    };
}

#endif