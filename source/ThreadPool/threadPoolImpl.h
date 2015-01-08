#ifndef _THREAD_POOL_IMPL_H_
#define _THREAD_POOL_IMPL_H_

#include "threadPool.h"
#include <exception>
#include <assert.h>
#include <vector>

#ifdef _LINUX_
#else
#	include <windows.h>
#endif

namespace ThreadPool
{
	struct tThreadPoolImpl
	{
		virtual void quit() = 0;

		virtual bool queueMsg(tKMsg* msg) = 0;

		virtual void init(int maxThread,tKMsgCumsummer* msgProcessor, bool autoBalance = true) = 0;

		virtual ~tThreadPoolImpl() = 0 {};
	};

	class kThreadPool : public tKMsgCumsummer,public IThreadPool
	{
	private:
		kThreadPool(kThreadPool const &);

		kThreadPool operator=(kThreadPool const &);

	public:
		kThreadPool(tThreadPoolImpl* threadPoolIpml)
			:tpImpl(threadPoolIpml)
		{
			assert(tpImpl != nullptr);
			tpImpl->init(6,this);
		}

		~kThreadPool()
		{
			quit();
			delete tpImpl;
			tpImpl = nullptr;
		}

	public:
		void quit() override	// IThreadPool::quit
		{
			tpImpl->quit();
		}

		void executeTask(KTASKSPTR task)  override	// IThreadPool::executeTask
		{
			tKMsg* tpMsg = MemoryManager::alloctMsg(EMSG_WORKLOAD,task);
			tpImpl->queueMsg(tpMsg);
		}

	public:
		bool processMsg(tKMsg* msg) override	//tKMsgCumsummer::processMsg
		{
			bool isExecuteNext = true;
			switch(msg->msgType)
			{
			case EMSG_POOLQUIT:
				{
					isExecuteNext = false;
				}
				break;
			case EMSG_WORKLOAD:
				{
					KTASKSPTR task = msg->workLoad;
					try
					{
						task->process();
					}catch(...)
					{
						std::exception_ptr ex = std::current_exception();
						task->except(ex);
					}
				}
				break;
			default:
				break;
			}

			MemoryManager::freeMsg(msg);
			return isExecuteNext;
		}

	private:
		tThreadPoolImpl* tpImpl;	//the real worker
	};

#ifdef _LINUX_
#	include "threadPool_linuxImpl.h"
#else
#	include "threadPool_winImpl.h"
#endif

}

#endif