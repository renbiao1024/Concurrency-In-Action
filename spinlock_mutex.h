#include <atomic>
#include <vector>
#include <iostream>
using namespace std;

//ԭ�Ӳ���ʵ��������
class spinlock_mutex
{
	atomic_flag flag;

public:
	spinlock_mutex():flag(ATOMIC_FLAG_INIT){}

	void lock()
	{
		while(flag.test_and_set(memory_order_acquire));
	}

	void unlock()
	{
		flag.clear(memory_order_release);
	}
};

//ǿ�ƴ��� �� ͬ������
vector<int>data;
atomic<bool> data_ready(false);

void read_thread()
{
	while (!data_ready.load()) //sleep ֱ�� write��
	{
		this_thread::sleep)chrono::milliseconds(1));
	}
	cout << data[0];
}

void write_thread()
{
	data.push_back(3);
	data_ready = true;
}
