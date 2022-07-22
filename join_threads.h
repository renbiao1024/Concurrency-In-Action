#include <vector>
#include <thread>
using namespace std;

class join_threads
{
	vector<thread>& threads;
public:
	explicit join_threads(vector<thread>& threads_):threads(threads_){}
	
	~join_threads()
	{
		for (unsigned long i = 0; i < threads.size(); ++i)
		{
			if (threads[i].joinable())
			{
				threads[i].join();
			}
		}
	}
};