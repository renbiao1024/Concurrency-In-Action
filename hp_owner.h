#include <atomic>
#include <functional>
usimg namespace std;

unsigned const max_hazard_pointers = 100;

struct hazard_pointer
{
	atomic<thread::id> id;
	atomic<void*> pointer;
};

hazard_pointer hazard_pointers[max_hazard_pointers];

class hp_owner
{
	hazard_pointer* hp;

public:
	hp_owner(hp_owner const&) = delete;
	hp_owner operator=(hp_owner const&) = delete;
	hp_owner() :hp(nullptr)
	{
	//搜索前述的定长数组，逐一查验配对的线程ID与风险指针，寻求尚未被占用的位置。
	//若发现某一位置并不隶属于任何线程，即把它收归己有。
	//判定和回收两项操作由compare_exchange_strong()一次完成。
	//如果compare_exchange_strong()执行失败，就表明别的线程抢先夺得所发现的位置，代码继续向前查找，
	//否则交换操作成功，当前线程便成功地占用该数组元素，遂将风险指针存入其中并停止查找（见break处）。
	//若搜遍整个定长数组也未能找到空闲的位置，则说明目前有太多线程正在使用风险指针，因此抛出异常。

		for (unsigned i = 0; i < max_hazard_pointers; ++i)
		{
			thread::id old_id;
			if (hazard_pointers[i].id.compare_exchange_strong(old_id, this_thread::get_id()))
			{
				hp = & hazard_pointers[i];
				break;
			}
		}
		if (!hp)
		{
			throw runtime_error("No hazard pointers available");
		}
	}

	atomic<void*>& get_pointer()
	{
		return hp->pointer;
	}

	~hp_owner()
	{
		hp->pointer.store(nullptr);
		hp->id.store(thread::id());
	}
};

atomic<void*>& get_hazard_pointer_for_current_thread()
{
	thread_local static hp_owner hazard;//存储当前线程的风险指针，并借以返回一个指针，由其真正指涉等待删除的节点。

	return hazard.get_pointer();
}

bool outstanding_hazard_pointers_for(void* p)
{
//扫描整个定长数组以查验对应的风险指针是否存在
	for (unsigned i = 0; i < max_hazard_pointers; ++i)
	{
		if (hazard_pointers[i].pointer.load() == p)
		{
			return true;
		}
	}
	return false;
}


template<typename T>
void do_delete(void* p)
{
	delete static_cast<T*> p;
}

struct data_to_reclaim
{
	void* data;
	function<void(void*)>deleter;
	data_to_reclaim* next;

	template <typename T>
	//该构造函数将待删除数据的指针视作void*型别存储到数据成员data中，然后用具现化的函数模板do_delete()充当函数指针，借以初始化成员deleter。
	//实质的删除操作则由简单的do_delete()函数执行，它先把参数的void*指针转换为选定的指针型别，再删除目标对象。
	//std::function<>将do_delete()作为函数指针妥善包装，并保存到成员deleter中，之后由data_to_reclaim的析构函数进行调用，借此删除数据
	data_to_reclaim(T* p):data(p),deleter(&do_delete<T>),next(0){}
	~data_to_reclaim(){delete(data); }
};

atomic<data_to_reclaim*>nodes_to_reclaim;

void add_to_reclaim_list(data_to_reclaim* node)
{
	node->next = nodes_to_reclaim.load();
	while(!nodes_to_reclaim.compare_exchange_weak(node->next,node));
}

//reclaim_later()向链表添加节点
template<typename T>
void reclaim_later(T* data)
{
	add_to_reclaim_list(new data_to_reclaim(data));
}


//delete_nodes_with_no_hazards()则扫描整个链表，以删除那些未被风险指针指涉的节点。
void delete_nodes_with_no_hazards()
{
	data_to_reclaim* current = nodes_to_reclaim.exchange(nullptr);
	//采用简单的exchange()操作，借此将整个nodes_to_reclaim链表收归当前线程所有。
	while (current)
	{
	//只要候删链表上存在节点，我们就逐一检查，判定它们是否被风险指针所指涉。
	//若未被指涉，该节点即可安全删除，其内含的数据也一并清理；
	//否则，我们将节点放回候删链表，留待以后回收。

		data_to_reclaim* const next = current->next;
		if (!outstanding_hazard_pointers_for(current->data))
		{
			delete current;
		}
		else
		{
			add_to_reclaim_list(current);
		}
		current = next;
	}
}