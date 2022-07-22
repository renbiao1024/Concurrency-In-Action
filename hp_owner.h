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
	//����ǰ���Ķ������飬��һ������Ե��߳�ID�����ָ�룬Ѱ����δ��ռ�õ�λ�á�
	//������ĳһλ�ò����������κ��̣߳��������չ鼺�С�
	//�ж��ͻ������������compare_exchange_strong()һ����ɡ�
	//���compare_exchange_strong()ִ��ʧ�ܣ��ͱ�������߳����ȶ�������ֵ�λ�ã����������ǰ���ң�
	//���򽻻������ɹ�����ǰ�̱߳�ɹ���ռ�ø�����Ԫ�أ��콫����ָ��������в�ֹͣ���ң���break������
	//���ѱ�������������Ҳδ���ҵ����е�λ�ã���˵��Ŀǰ��̫���߳�����ʹ�÷���ָ�룬����׳��쳣��

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
	thread_local static hp_owner hazard;//�洢��ǰ�̵߳ķ���ָ�룬�����Է���һ��ָ�룬��������ָ��ȴ�ɾ���Ľڵ㡣

	return hazard.get_pointer();
}

bool outstanding_hazard_pointers_for(void* p)
{
//ɨ���������������Բ����Ӧ�ķ���ָ���Ƿ����
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
	//�ù��캯������ɾ�����ݵ�ָ������void*�ͱ�洢�����ݳ�Աdata�У�Ȼ���þ��ֻ��ĺ���ģ��do_delete()�䵱����ָ�룬���Գ�ʼ����Աdeleter��
	//ʵ�ʵ�ɾ���������ɼ򵥵�do_delete()����ִ�У����ȰѲ�����void*ָ��ת��Ϊѡ����ָ���ͱ���ɾ��Ŀ�����
	//std::function<>��do_delete()��Ϊ����ָ�����ư�װ�������浽��Աdeleter�У�֮����data_to_reclaim�������������е��ã����ɾ������
	data_to_reclaim(T* p):data(p),deleter(&do_delete<T>),next(0){}
	~data_to_reclaim(){delete(data); }
};

atomic<data_to_reclaim*>nodes_to_reclaim;

void add_to_reclaim_list(data_to_reclaim* node)
{
	node->next = nodes_to_reclaim.load();
	while(!nodes_to_reclaim.compare_exchange_weak(node->next,node));
}

//reclaim_later()��������ӽڵ�
template<typename T>
void reclaim_later(T* data)
{
	add_to_reclaim_list(new data_to_reclaim(data));
}


//delete_nodes_with_no_hazards()��ɨ������������ɾ����Щδ������ָ��ָ��Ľڵ㡣
void delete_nodes_with_no_hazards()
{
	data_to_reclaim* current = nodes_to_reclaim.exchange(nullptr);
	//���ü򵥵�exchange()��������˽�����nodes_to_reclaim�����չ鵱ǰ�߳����С�
	while (current)
	{
	//ֻҪ��ɾ�����ϴ��ڽڵ㣬���Ǿ���һ��飬�ж������Ƿ񱻷���ָ����ָ�档
	//��δ��ָ�棬�ýڵ㼴�ɰ�ȫɾ�������ں�������Ҳһ������
	//�������ǽ��ڵ�Żغ�ɾ���������Ժ���ա�

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