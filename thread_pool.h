//数目固定的线程池
#include <vector>
#include <thread>
#include <atomic>
#include "threadsafe_queue.h"
#include <functional>
#include "join_threads.h"

class thread_pool
{
	atomic_bool done;
	threadsafe_queue<function<void()>> work_queue;
	vector<thread> threads;
	join_threads joiner;

	void worker_thread()
	{
	//worker_thread()函数本身相当简单：各线程通过它反复从队列中取出任务，并同时执行，只要done标志未被设置为成立就一直循环。
	//假如队列中没有任务，该函数便调用std::this_thread::yield()令当前线程稍事歇息，好让其他线程把任务放入队列，
	//随后它再切换回来，继续下一轮循环，尝试从队列中领取任务运行。

		while (!done)
		{
			function<void()>task;
			if (work_queue.try_pop(task))
			{
				task();
			}
			else
			{
				this_thread::yield();
			}
		}
	}

public:
	thread_pool() :done(false), joiner(threads)
	{
		unsigned const thread_count = thread::hardware_concurrency();
		try
		{
			for (unsigned i = 0; i < thread_count; ++i)
			{
				threads.push_back(thread(&thread_pool::worker_thread, this));//把当前线程存到threads,调用 worker_thread
			}
		}
		catch (...)// catch (…)能够捕获多种数据类型的异常对象
		{
			done = =true;
			throw;
		}
	}
	~thread_pool()
	{
		done = true;
	}

	template<typename FunctionType>
	void submit(FunctionType f)
	{
		work_queue.push(function<void()>(f));
	}

};
