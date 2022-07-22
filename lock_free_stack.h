#include <stack>
#include <atomic>
#include <memory>
#include "hp_owner"
using namespace std;

template<typename T>
class lock_free_stack 
{
private:
	struct node
	{
		T data;
		node* next;
		node(T const& data_): data(data_){}
	};

	atomic<node*> head;

	atomic<unsigned> threads_in_pop;//原子pop计数器
	atomic<node*>to_be_deleted;

public:
	void push(T const& data)
	{
		node* new_node = new node(data);
		new_node->next = head.load();

		//运用compare_exchange_weak()做判断，确定head指针与new_node->next所存储的值是否依然相同，若相同，就将head指针改为指向new_node。
		//若它返回false，则表明对比的两个指针互异（head指针被其他线程改动过），第一个参数new_node->next就被更新成head指针的当前值。
		while(!head.compare_exchange_weak(new_node->next,new_node));
	}
////pop节点

////方法一：通过 待删链表 实现的无锁栈

//public:
//	//维护一个“等待删除链表”（简称“候删链表”），每次执行弹出操作都向它加入相关节点，等到没有线程调用pop()时，才删除候删链表中的节点。
//	//如何得知目前没有线程调用pop()？答案很简单，即对调用进行计数。如果为pop()函数设置一个计数器，使之在进入函数时自增，在离开函数时自减，
//	//那么，当计数变为0时，我们就能安全删除候删链表中的节点。该计数器必须原子化，才可以安全地接受多线程访问。
//	shared_ptr<T>pop()
//	{
//		++ threads_in_pop;
//		node* old_head = head.load();
//		while(old_head && !head.compare_exchange_weak(head,head->next)); //判空 && 找到最新的head
//		shared_ptr<T>res;
//		if(old_head)
//			res.swap(old_head->data); // 通过swap()变换共享指针data来删除实质数据，*res = old_head->data
//		try_reclaim(old_head); // 内部让 threads_in_pop 自减
//		return res;
//	}
//
//private:
//	// 删除 “等待删除链表”里的nodes
//	static void delete_nodes(node* nodes)
//	{
//		while (nodes)
//		{
//			node* next = nodes->next;
//			delete nodes;
//			nodes = next;
//		}
//	}
//
//	void chain_pending_nodes(node* first, node* last)
//	{
//		last->next = to_be_deleted;
//
//		while(!to_be_deleted.compare_exchange_weak(last->next, first));
//	}
//
//	void chain_pending_node(node* n)
//	{
//		chain_pending_nodes(n,n);
//	}
//
//	void chain_pending_nodes(node* nodes)
//	{
//		node* last = nodes;
//		while (node* const next = last->next)//找到 侯删链表 的末端
//		{
//			last = next;
//		}
//		chain_pending_nodes(nodes, last);
//	}
//
//	void try_reclaim(node* old_head)//回收
//	{
//		if (threads_in_pop == 1) //仅当前线程调用pop，那么我们就可以安全删除节点
//		{
//			node* nodes_to_delete = to_be_deleted.exchange(nullptr);
//			//如果自减后计数器变成0，我们便知晓，别的线程不会访问候删链表中的节点。
//			//这时有可能出现新的待删除的节点，需加入候删链表中
//			if (!--threads_in_pop)
//			{
//				delete_nodes(nodes_to_delete);//遍历候删链表，逐一删除等待删除的节点。
//			}
//			else if (nodes_to_delete)
//			{
//				chain_pending_nodes(nodes_to_delete); //有其他线程在引用，我们将其放回候删链表，等待其引用完成
//			}
//			delete old_head;
//		}
//		else // 如果计数器不为1 说明删除节点行为不安全，故将其添加到待删链表
//		{
//			chain_pending_node(old_head);
//			--threads_in_pop;
//		}
//	}

//方法二：运用风险指针 检测无法回收的 节点

//如果一个数据还被其他线程所使用，删除它就有风险，用风险指针标记，不允许删除
//用到的函数 见 hp_owner.h
public:
	shared_ptr<T> pop()
	{
		atomic<void*>& hp = get_hazard_pointer_for_current_thread();//生成风险指针 返回其引用
		//先读取旧的head指针，然后设置风险指针，中间可能存在时间空隙，所以风险指针的设置操作必须配合while循环，从而确保目标节点不会在间隙内删除。
		//在这一时间窗口内，其他线程均无法得知当前线程正在访问该节点。
		//碰巧，如果旧的头节点需被删除，则head指针本身必然会被改动。
		//因此，只要head指针与风险指针的目标不一致，我们便不断循环对比两者，直到它们变成一致为止。
		node* old_head = head.load();
		do {
			
			node* temp;
			do { //反复循环 知道风险指针被设为 head为止
				temp = old_head;
				hp.store(old_head);
				old_head = head.load();
			} while (old_head != temp);
		}while(old_head && !head.compare_exhange_strong(old_head, old_head->next));
		hp.store(nullptr);//一旦更新了 head 指针 就把风险指针清零

		//如果节点成功弹出，其节点指针便与隶属其他线程的风险指针逐一对比，从而判定它是否被别的线程指涉。
		//若被指涉，该节点就不能马上删除，而必须放置到候删链表中留待稍后回收；否则，我们立刻删除它。
		//最后，我们调用delete_nodes_with_no_hazards()，以核查由reclaim_later()回收的所有节点，
		//如果其中有一些节点不再被任何风险指针所指涉，即可安全删除。
		//剩余的节点则依然被风险指针所指涉，在下一个线程调用pop()时，它们会再次按同样的方式被处理。
		shared_ptr<T> res;
		if (old_head)
		{
			res.swap(old_head->data);
			if (outstanding_hazard_pointers_for(old_head))//删除旧的头节点之前，先检查她是否被风险指针所涉及
			{
				reclaim_later(old_head);
			}
			else
			{
				delete old_head;
			}
			delete_nodes_with_no_hazards();
		}
		return res;
	}
//每当调用pop()时，代码就要扫描风险指针数组，查验其中的原子变量，其数目多达max_hazard_pointers。
//原子操作本来就很慢，在台式计算机的CPU上，等效的非原子操作通常比它快约100倍，这令pop()操作开销高昂。
//我们既要遍历候删链表中的全部节点，还要针对每个节点扫描整个风险指针数组。

};

////方法三：借引用计数检测正在使用的节点
template<typename T>
class lock_free_stack_by_shared_ptr
{
private:
	struct node
	{
		shared_ptr<T> data;
		shared_ptr<node>next;
		node(T const& data_):data(make_shared<T>(data_)){}
	};

	shared_ptr<node> head;

public:
	void push(T const& data)
	{
		shared_ptr<node>const new_node = make_shared<node>(data);
		new_node->next = atomic_load(&head);
		while(!atomic_compare_exchange_weak(&head, &new_node->next, new_node));
	}

	shared_ptr<T> pop()
	{
		shared_ptr<node> old_head = atomic_load(&head);
		while(old_head && !atomic_compare_exchange_weak(&head, &old_head, atomic_load(&old_head->next)));
		if(old_head){
			atomic_store(&old_head->next, shared_ptr<node>()) 
			return old_head->data;
		}
		return shared_ptr<T>();
	}

	~lock_free_stack_by_shared_ptr()
	{
		while(pop());
	}
};