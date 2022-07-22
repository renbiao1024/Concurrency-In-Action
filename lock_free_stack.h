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

	atomic<unsigned> threads_in_pop;//ԭ��pop������
	atomic<node*>to_be_deleted;

public:
	void push(T const& data)
	{
		node* new_node = new node(data);
		new_node->next = head.load();

		//����compare_exchange_weak()���жϣ�ȷ��headָ����new_node->next���洢��ֵ�Ƿ���Ȼ��ͬ������ͬ���ͽ�headָ���Ϊָ��new_node��
		//��������false��������Աȵ�����ָ�뻥�죨headָ�뱻�����̸߳Ķ���������һ������new_node->next�ͱ����³�headָ��ĵ�ǰֵ��
		while(!head.compare_exchange_weak(new_node->next,new_node));
	}
////pop�ڵ�

////����һ��ͨ�� ��ɾ���� ʵ�ֵ�����ջ

//public:
//	//ά��һ�����ȴ�ɾ����������ơ���ɾ��������ÿ��ִ�е�������������������ؽڵ㣬�ȵ�û���̵߳���pop()ʱ����ɾ����ɾ�����еĽڵ㡣
//	//��ε�֪Ŀǰû���̵߳���pop()���𰸺ܼ򵥣����Ե��ý��м��������Ϊpop()��������һ����������ʹ֮�ڽ��뺯��ʱ���������뿪����ʱ�Լ���
//	//��ô����������Ϊ0ʱ�����Ǿ��ܰ�ȫɾ����ɾ�����еĽڵ㡣�ü���������ԭ�ӻ����ſ��԰�ȫ�ؽ��ܶ��̷߳��ʡ�
//	shared_ptr<T>pop()
//	{
//		++ threads_in_pop;
//		node* old_head = head.load();
//		while(old_head && !head.compare_exchange_weak(head,head->next)); //�п� && �ҵ����µ�head
//		shared_ptr<T>res;
//		if(old_head)
//			res.swap(old_head->data); // ͨ��swap()�任����ָ��data��ɾ��ʵ�����ݣ�*res = old_head->data
//		try_reclaim(old_head); // �ڲ��� threads_in_pop �Լ�
//		return res;
//	}
//
//private:
//	// ɾ�� ���ȴ�ɾ���������nodes
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
//		while (node* const next = last->next)//�ҵ� ��ɾ���� ��ĩ��
//		{
//			last = next;
//		}
//		chain_pending_nodes(nodes, last);
//	}
//
//	void try_reclaim(node* old_head)//����
//	{
//		if (threads_in_pop == 1) //����ǰ�̵߳���pop����ô���ǾͿ��԰�ȫɾ���ڵ�
//		{
//			node* nodes_to_delete = to_be_deleted.exchange(nullptr);
//			//����Լ�����������0�����Ǳ�֪��������̲߳�����ʺ�ɾ�����еĽڵ㡣
//			//��ʱ�п��ܳ����µĴ�ɾ���Ľڵ㣬������ɾ������
//			if (!--threads_in_pop)
//			{
//				delete_nodes(nodes_to_delete);//������ɾ������һɾ���ȴ�ɾ���Ľڵ㡣
//			}
//			else if (nodes_to_delete)
//			{
//				chain_pending_nodes(nodes_to_delete); //�������߳������ã����ǽ���Żغ�ɾ�����ȴ����������
//			}
//			delete old_head;
//		}
//		else // �����������Ϊ1 ˵��ɾ���ڵ���Ϊ����ȫ���ʽ�����ӵ���ɾ����
//		{
//			chain_pending_node(old_head);
//			--threads_in_pop;
//		}
//	}

//�����������÷���ָ�� ����޷����յ� �ڵ�

//���һ�����ݻ��������߳���ʹ�ã�ɾ�������з��գ��÷���ָ���ǣ�������ɾ��
//�õ��ĺ��� �� hp_owner.h
public:
	shared_ptr<T> pop()
	{
		atomic<void*>& hp = get_hazard_pointer_for_current_thread();//���ɷ���ָ�� ����������
		//�ȶ�ȡ�ɵ�headָ�룬Ȼ�����÷���ָ�룬�м���ܴ���ʱ���϶�����Է���ָ������ò����������whileѭ�����Ӷ�ȷ��Ŀ��ڵ㲻���ڼ�϶��ɾ����
		//����һʱ�䴰���ڣ������߳̾��޷���֪��ǰ�߳����ڷ��ʸýڵ㡣
		//���ɣ�����ɵ�ͷ�ڵ��豻ɾ������headָ�뱾���Ȼ�ᱻ�Ķ���
		//��ˣ�ֻҪheadָ�������ָ���Ŀ�겻һ�£����Ǳ㲻��ѭ���Ա����ߣ�ֱ�����Ǳ��һ��Ϊֹ��
		node* old_head = head.load();
		do {
			
			node* temp;
			do { //����ѭ�� ֪������ָ�뱻��Ϊ headΪֹ
				temp = old_head;
				hp.store(old_head);
				old_head = head.load();
			} while (old_head != temp);
		}while(old_head && !head.compare_exhange_strong(old_head, old_head->next));
		hp.store(nullptr);//һ�������� head ָ�� �Ͱѷ���ָ������

		//����ڵ�ɹ���������ڵ�ָ��������������̵߳ķ���ָ����һ�Աȣ��Ӷ��ж����Ƿ񱻱���߳�ָ�档
		//����ָ�棬�ýڵ�Ͳ�������ɾ������������õ���ɾ�����������Ժ���գ�������������ɾ������
		//������ǵ���delete_nodes_with_no_hazards()���Ժ˲���reclaim_later()���յ����нڵ㣬
		//���������һЩ�ڵ㲻�ٱ��κη���ָ����ָ�棬���ɰ�ȫɾ����
		//ʣ��Ľڵ�����Ȼ������ָ����ָ�棬����һ���̵߳���pop()ʱ�����ǻ��ٴΰ�ͬ���ķ�ʽ������
		shared_ptr<T> res;
		if (old_head)
		{
			res.swap(old_head->data);
			if (outstanding_hazard_pointers_for(old_head))//ɾ���ɵ�ͷ�ڵ�֮ǰ���ȼ�����Ƿ񱻷���ָ�����漰
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
//ÿ������pop()ʱ�������Ҫɨ�����ָ�����飬�������е�ԭ�ӱ���������Ŀ���max_hazard_pointers��
//ԭ�Ӳ��������ͺ�������̨ʽ�������CPU�ϣ���Ч�ķ�ԭ�Ӳ���ͨ��������Լ100��������pop()���������߰���
//���Ǽ�Ҫ������ɾ�����е�ȫ���ڵ㣬��Ҫ���ÿ���ڵ�ɨ����������ָ�����顣

};

////�������������ü����������ʹ�õĽڵ�
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