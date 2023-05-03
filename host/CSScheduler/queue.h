#ifndef __QUEUE_H_
#define __QUEUE_H_

#include "Mutex.h"
#include <list>


template<class T>
class Queue
{
	list<T> data;
	Mutex m_Mutex;
public:
	Queue(){ }

	void enqueue(T ptr)
	{
		m_Mutex.lock();

		data.push_back(ptr);

		m_Mutex.unlock();
	}

	T dequeue()
	{
		m_Mutex.lock();

		T ptr = data.front();
		data.pop_front();

		m_Mutex.unlock();

		return ptr;
	}

	int size()
	{
		return data.size();
	}
};

#endif
