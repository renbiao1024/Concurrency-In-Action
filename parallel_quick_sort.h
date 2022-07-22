#include <thread>
#include <future>
#include <list>
#include "threadsafe_stack.h"

using namespace std;

template<typename T>
struct sorter
{
	struct chunk_to_sort
	{
		list<T> data;
		promise<list<T>> promise;
	};

	threadsafe_stack<chunk_to_sort>chunks;
	vector<thread>threads;
	unsigned const max_thread_count;
	atomic<bool> end_of_data;

	//线程数目受限于std::thread::hardware_concurrency()的返回值，因此避免了过度的任务切换。
	sorter():max_thread_count(thread::hardware_concurrency()-1), end_of_data(false){}

	//析构函数将设置标志end_of_data成立，然后等待全部线程结束。标志的成立使得线程函数内的循环终止。
	//由sorter类的析构函数汇合各个线程。
	~sorter() {
		end_of_data = true;
		for (unsigned i = 0; i < threads.size(); ++i)
		{
			threads[i].join();
		}
	}

	void try_sort_chunk()
	{
		//try_sort_chunk()先从栈容器弹出一段数据并对其进行排序，再把结果存入附属该段的promise中，使之准备就绪，以待提取。
		//向栈容器压入数据段与取出相关结果相互对应，两项操作均由同一个线程先后执行
		shared_ptr<chunk_to_sort> chunk = chunks.pop();
		if (chunk)
		{
			sort_chunk(chunk);
		}
	}

	list<T> do_sort(list<T>& chunk_data)
	{
		if(chunk_data.empty())
			return chunk_data;
		list<T> result;
		result.splice(result.begin(),chunk_data,chunk_data.begin());
		T const& partition_val = *result.begin();
		//借partition完成数据分段
		typename list<T>::iterator divide_point = partition(chunk_data.begin(),chunk_data.end(),
			[&](T const& val){return val < partition_val;});

		chunk_to_sort new_lower_chunk;
		new_lower_chunk.data.splice(new_lower_chunk.data.end(), chunk_data, chunk_data.begin(), divide_point);
		future<list<T>>new_lower = new_lower_chunk.promise.get_future();
		chunks.push(move(new_lower_chunk));
		if (threads.size() < max_thread_count)
		{
			threads.push_back(thread(&sorter<T>::sort_thread, this));
		}
		list<T> new_higher(do_sort(chunk_data));
		result.splice(result.end(), new_higher);

		//在当前线程的等待期间，我们让它试着从栈容器取出数据进行处理。
		while (new_lower.wait_for(chrono::seconds(0)) != future_status::ready)
		{
			try_sort_chunk();
		}

		result.splice(result.begin(), new_lower.get());
		return result;
	}

	void sort_chunk(shared_ptr<chunk_to_sort> const& chunk)
	{
		chunk->promise.set_value(do_sort(chunk->data));
	}

	void sort_thread()
	{
	//只要标志end_of_data没有成立，各线程便反复循环，尝试对栈内数据段进行排序。
	//每个线程在两次检测标志之间进行让步，好让别的线程有机会向栈容器添加数据段。
		while (!end_of_data)
		{
			try_sort_chunk();
			this_thread::yield();
		}
	}
};

template<typename T>
list<T> parallel_quick_sort(list<T> input)
{
	//do_sort()将在全部数据段都完成排序后返回（即便许多工作线程仍在运行），主线程进而从parallel_quick_sort()的调用返回，并销毁sorter对象。
	if(input.empty()) return input;
	sorter<T> s;
	return s.do_sort(input);
}