/**********************************************************************************************************************
定长内存池 ，无锁环形队列。 忽略掉TLBF_STAT状态
 作者：eset，qq：259520469
 **********************************************************************************************************************/
#ifndef TES_LOCKFREE_OBJ_FIFO_H
#define TES_LOCKFREE_OBJ_FIFO_H
#include <iostream>


struct Tes_Memory_Outcome {
	bool segmentflag;
	int m_size1;
	int m_size2;
	int m_size; //请求的内存大小
	void* m_ptr1;
	void* m_ptr2;
	int pre_token;
	int end_token;
};
//lockfree  obj subclass
class Tes_LF_Node {
public:
	int id;
	void* pre;
	void* next;
};



template<class T>
class Tes_LockFree_Obj_Fifo {
public:
	Tes_LockFree_Obj_Fifo() {
		m_time = m_size = 0;
		m_cls = NULL;
	};
	virtual ~Tes_LockFree_Obj_Fifo() {
		if (m_cls != NULL) {
			delete[] m_cls;
		}
	};
public:
	enum TLBF_STAT {
		Empty, //空
		Idle, //闲
		MBuys, //生产申请不到内存
		TBuys, //队列忙 申请不到资源
		ErrParm, //错误的计算参数
		ThreadRace, //线程竞争
	};
	bool Ini_Size(int tsize);
protected:
public:
	TLBF_STAT m_stat;
	int m_time; //大循环的次数
	int m_size;
	T* m_cls;
	std::atomic<int> tail_cursor; //尝试性的下标 尾巴
	std::atomic<int> font_cursor; //尝试性的下标 头
	std::atomic<int> i_index; // 实际下标 in
	std::atomic<int> o_index; //实际下标  out
public:
	int Get_Empty();
	int Get_Len();
	//生产行为
private:
	bool befproduce(Tes_Memory_Outcome& obj_info); //预先 生产行为
public:
	bool produce(Tes_Memory_Outcome& obj_info);
	bool set_produce_end(Tes_Memory_Outcome& obj_info);
	//消费行为
private:
	bool befconsume(Tes_Memory_Outcome& obj_info); //预先消费行为
public:
	int get_capacity() {
		return m_size;
	}
	bool consume(Tes_Memory_Outcome& obj_info);
	bool set_consume_end(Tes_Memory_Outcome& obj_info);
	TLBF_STAT geterr() {
		return m_stat;
	}
};
template<class T>
bool Tes_LockFree_Obj_Fifo<T>::Ini_Size(int tsize) //初始化内存空间 并形成环
{
	if (tsize == 0) {
		m_stat = TLBF_STAT::ErrParm;
		return false;
	}
	m_cls = new T[tsize];
	if (m_cls == NULL) {
		m_stat = TLBF_STAT::ErrParm;
		return false;
	} else {
		m_cls[0].pre = (T*) (&m_cls[tsize - 1]);
		m_cls[tsize - 1].next = (T*) (&m_cls[0]);
		for (int i = 0; i < tsize - 1; i++) {
			m_cls[i].id = i;
			T& a = m_cls[i];
			T& b = m_cls[i + 1];
			a.next = (T*) (&b);
			b.pre = (T*) (&a);
		}
		m_cls[tsize - 1].id = tsize - 1;
		m_size = tsize;
		m_time = 0;
		tail_cursor = 0;
		font_cursor = 0;
		i_index = 0;
		o_index = 0;
		return true;
	}
}
template<class T>
int Tes_LockFree_Obj_Fifo<T>::Get_Empty() {
	int oi = o_index.load();
	int fc = font_cursor.load();
	if (oi == fc) {
		return m_size - 1;
	} else if (oi > fc) {
		return (oi - fc) - 1;
	} else {
		return m_size - (fc - oi) - 1;
	}
}
template<class T>
bool Tes_LockFree_Obj_Fifo<T>::befproduce(Tes_Memory_Outcome& obj_info) {
	int fc = font_cursor.load();
	int sz = Get_Empty();
	if (obj_info.m_size >= sz) {
		m_stat = TLBF_STAT::MBuys;
		return false; //size not enough
	} else {
		//start cal
		obj_info.segmentflag = false;
		obj_info.m_size1 = obj_info.m_size;
		obj_info.m_ptr1 = (void*) (&m_cls[fc]);
		obj_info.end_token = (fc + obj_info.m_size) % m_size;
		return true;
	}
}
template<class T>
bool Tes_LockFree_Obj_Fifo<T>::produce(Tes_Memory_Outcome& obj_info) {
	int brokenFlag = 0;
	do {
		obj_info.pre_token = font_cursor.load(); //多个线程同时分配到这个
		if (brokenFlag > 12) {
			m_stat = TLBF_STAT::TBuys;
			std::cout<<"busy"<<std::endl;
		}
		if (obj_info.m_size == 0) {
			m_stat = TLBF_STAT::ErrParm;
			return false;
		}
		if (!befproduce(obj_info)) {
			m_stat = TLBF_STAT::MBuys;
			return false;
		} else {
			m_stat = TLBF_STAT::Idle;
		}
		brokenFlag++;
	} while (!font_cursor.compare_exchange_weak(obj_info.pre_token,
			obj_info.end_token));
	if (obj_info.segmentflag) {
		m_time++;
	}
	return true;
}
template<class T>
bool Tes_LockFree_Obj_Fifo<T>::set_produce_end(Tes_Memory_Outcome& obj_info) {
	int step = 0;
	int dt;
	do {
		dt = obj_info.pre_token;
		if (step > 12) {
			m_stat = TLBF_STAT::TBuys;
		}
		step++;
	} while (!i_index.compare_exchange_weak(dt,
			obj_info.end_token));
	return true;
}
template<class T>
int Tes_LockFree_Obj_Fifo<T>::Get_Len() {
	int ii = i_index.load();
	int tc=tail_cursor.load();
	if (ii == tc) {
		return 0;
	} else if (ii > tc) {
		return (ii - tc);
	} else {
		return m_size - (tc - ii);
	}
}
template<class T>
bool Tes_LockFree_Obj_Fifo<T>::befconsume(Tes_Memory_Outcome& obj_info) {
	//start cal
	int sz = Get_Len();
	if (0 == sz) {
		m_stat = TLBF_STAT::Empty;
		return false; //size 0
	} else {
		//start cal
		obj_info.segmentflag = false;
		obj_info.m_size1 = sz;
		obj_info.m_ptr1 = (void*) (&m_cls[tail_cursor.load()]);
		obj_info.end_token = (tail_cursor.load() + sz) % m_size;
		return true;
	}
}
template<class T>
bool Tes_LockFree_Obj_Fifo<T>::consume(Tes_Memory_Outcome& obj_info) {
	//start cal
	int brokenFlag = 0;
	do {
		obj_info.pre_token = tail_cursor.load();
		if (brokenFlag > 12) {
			m_stat = TLBF_STAT::TBuys;
			std::cout<<"busy"<<std::endl;
		}
		if (befconsume(obj_info)) {
			m_stat = TLBF_STAT::Idle;
		} else {
			m_stat = TLBF_STAT::Empty;
			return false;
		}
		brokenFlag++;
	} while (!tail_cursor.compare_exchange_weak(obj_info.pre_token,
			obj_info.end_token));
	return true;
}
template<class T>
bool Tes_LockFree_Obj_Fifo<T>::set_consume_end(Tes_Memory_Outcome& obj_info) {
	int step = 0;
	int dt;
	do {
		dt = obj_info.pre_token;
		if (step > 12) {
			m_stat = TLBF_STAT::TBuys;
		}
		step++;
	} while (!o_index.compare_exchange_weak(dt,
			obj_info.end_token));
	return true;
}

#endif // TES_LOCKFREE_OBJ_FIFO_H
