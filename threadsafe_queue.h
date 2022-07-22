#pragma once
#include <mutex>
#include <condition_variable>
#include <exception>
#include <queue>

template <typename T>
class threadsafe_queue
{
private:
	mutable std::mutex m;
	std::queue<T> data_queue;
	std::condition_variable cond;

public:
	threadsafe_queue(){};

	void push(T new_val)
	{
		std::lock_guard<std::mutex> lk(m);
		data_queue.push(std::move(new_val));
		cond.notify_one(); // push完 唤醒一个其他线程，然后自己解锁
	}

	//wait_pop 是为了 等待-通知 功能
	void wait_pop(T& res)
	{
		std::unique_lock<std::mutex> lk(m);
		//wait() 阻塞，直到底层的队列容器中出现最少一个数据才返回，所以我们不必忧虑队列为空的状况；
		//又因为互斥已经加锁，在等待期间数据仍然受到保护，所以这两个函数不会引入任何数据竞争，死锁也不可能出现，不变量保持成立。
		cond.wait(lk, [this]{ return !data_queue.empty();});

		res = std::move(data_queue.front());
		data_queue.pop();
	}

	std::shared_ptr<T> wait_pop()
	{
		std::unique_lock<std::mutex> lk(m);
		cond.wait(lk, [this] { return !data_queue.empty();});
		shared_ptr<T> res (std::make_shared(std::move(data_queue.front())));
		data_queue.pop();
		return res;
	}

	bool try_pop(T& res)
	{
		std::lock_guard<std::mutex> lk(m);
		if (data_queue.empty()) return false;
		res = std::move(data_queue.front());
		data_queue.pop();
		return true;
	}

	std::shared_ptr<T> try_pop()
	{
		std::lock_guard<std::mutex> lk(m);
		if(data_queue,empty()) return std::shared_ptr<T>();
		std::shared_ptr<T> res (std::make_shared(std::move(data_queue.front())));
		data_queue.pop();
		return res;
	}

	bool empty() const
	{
		std::lock_guard<std::mutex>lock(m);
		return data_queue.empty();
	}
};

//假定在数据压入队列的过程中，有多个线程同时在等待，那么data_cond.notify_one()的调用只会唤醒其中一个。
// 然而，若该觉醒的线程在执行wait_and_pop()时抛出异常（譬如新指针std::shared_ptr<>在构建时就有可能产生异常），就不会有任何其他线程被唤醒。
//第一种处理方法是，data_cond.notify_one()改为data_cond.notify_all()。这样就会唤醒全体线程，但要大大增加开销：它们绝大多数还是会发现队列依然为空，只好重新休眠。
//第二种处理方式是，倘若有异常抛出，则在wait_and_pop()中再次调用notify_one()，从而再唤醒另一线程，让它去获取存储的值。
//第三种处理方式是，将std::shared_ptr<>的初始化语句移动到push()的调用处，令队列容器改为存储std::shared_ptr<>，而不再直接存储数据的值。
	//从内部std::queue<>复制std::shared_ptr<>实例的操作不会抛出异常，所以wait_and_pop()也是异常安全的。


//第三种处理方式
template <typename T>
class threadsafe_queue_2
{
private:
	mutable std::mutex m;
	std::queue<std::shared_ptr<T>> data_queue;
	std::condition_variable cond;

public:
	threadsafe_queue_2() {};

	void push(T new_val)
	{
		std::shared_ptr<T> data(std::make_shared(std::move(new_val)));
		std::lock_guard<std::mutex> lk(m);
		data_queue.push(data);
		cond.notify_one();
	}

	void wait_pop(T& res)
	{
		std::unique_lock<std::mutex> lk(m);
		cond.wait(lk, [this] { return !data_queue.empty();});
		res = std::move(*data_queue.front());
		data_queue.pop();
	}

	std::shared_ptr<T> wait_pop()
	{
		std::unique_lock<std::mutex> lk(m);
		cond.wait(lk, [this] { return !data_queue.empty();});
		shared_ptr<T> res = data_queue.front();
		data_queue.pop();
		return res;
	}

	bool try_pop(T& res)
	{
		std::lock_guard<std::mutex> lk(m);
		if (data_queue.empty()) return false;
		res = std::move(*data_queue.front());
		data_queue.pop();
		return true;
	}

	std::shared_ptr<T> try_pop()
	{
		std::lock_guard<std::mutex> lk(m);
		if (data_queue, empty()) return std::shared_ptr<T>();
		std::shared_ptr<T> res = data_queue.front();
		data_queue.pop();
		return res;
	}

	bool empty() const
	{
		std::lock_guard<std::mutex>lock(m);
		return data_queue.empty();
	}
};
