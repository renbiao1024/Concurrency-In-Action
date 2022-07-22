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
		cond.notify_one(); // push�� ����һ�������̣߳�Ȼ���Լ�����
	}

	//wait_pop ��Ϊ�� �ȴ�-֪ͨ ����
	void wait_pop(T& res)
	{
		std::unique_lock<std::mutex> lk(m);
		//wait() ������ֱ���ײ�Ķ��������г�������һ�����ݲŷ��أ��������ǲ������Ƕ���Ϊ�յ�״����
		//����Ϊ�����Ѿ��������ڵȴ��ڼ�������Ȼ�ܵ������������������������������κ����ݾ���������Ҳ�����ܳ��֣����������ֳ�����
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

//�ٶ�������ѹ����еĹ����У��ж���߳�ͬʱ�ڵȴ�����ôdata_cond.notify_one()�ĵ���ֻ�ỽ������һ����
// Ȼ�������þ��ѵ��߳���ִ��wait_and_pop()ʱ�׳��쳣��Ʃ����ָ��std::shared_ptr<>�ڹ���ʱ���п��ܲ����쳣�����Ͳ������κ������̱߳����ѡ�
//��һ�ִ������ǣ�data_cond.notify_one()��Ϊdata_cond.notify_all()�������ͻỽ��ȫ���̣߳���Ҫ������ӿ��������Ǿ���������ǻᷢ�ֶ�����ȻΪ�գ�ֻ���������ߡ�
//�ڶ��ִ���ʽ�ǣ��������쳣�׳�������wait_and_pop()���ٴε���notify_one()���Ӷ��ٻ�����һ�̣߳�����ȥ��ȡ�洢��ֵ��
//�����ִ���ʽ�ǣ���std::shared_ptr<>�ĳ�ʼ������ƶ���push()�ĵ��ô��������������Ϊ�洢std::shared_ptr<>��������ֱ�Ӵ洢���ݵ�ֵ��
	//���ڲ�std::queue<>����std::shared_ptr<>ʵ���Ĳ��������׳��쳣������wait_and_pop()Ҳ���쳣��ȫ�ġ�


//�����ִ���ʽ
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
