//��Ŀ�̶����̳߳�
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
	//worker_thread()���������൱�򵥣����߳�ͨ���������Ӷ�����ȡ�����񣬲�ͬʱִ�У�ֻҪdone��־δ������Ϊ������һֱѭ����
	//���������û�����񣬸ú��������std::this_thread::yield()�ǰ�߳�����ЪϢ�����������̰߳����������У�
	//��������л�������������һ��ѭ�������ԴӶ�������ȡ�������С�

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
				threads.push_back(thread(&thread_pool::worker_thread, this));//�ѵ�ǰ�̴߳浽threads,���� worker_thread
			}
		}
		catch (...)// catch (��)�ܹ���������������͵��쳣����
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
