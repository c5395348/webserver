#ifndef MYSERVER_THREADPOOL_H
#define MYSERVER_THREADPOOL_H

#include<list>
#include<cstdio>
#include<exception>
#include<pthread.h>
#include"locker.h"

template<typename T>
class myserver_threadpool
{
	public:
	myserver_threadpool(int thread_number=0,int max_requests=10000);
	~myserver_threadpool();
	bool append(T* request);
	private:
	static void* worker(void* arg);
	void run();
	private:
	int m_thread_number;
	int m_max_requests;
	pthread_t* m_threads;
	std::list<T* > m_workqueue;
	locker m_queuelocker;
	sem m_queuestat;
	bool m_stop;
};

template<typename T>
myserver_threadpool<T>::myserver_threadpool(int thread_number,int max_requests):m_thread_number(thread_number),m_max_requests(max_requests),m_stop(false),m_threads(NULL)
{
	if((thread_number<=0)||(max_requests<=0))
	{
		printf("error1\n");
		throw std::exception();
	}
	m_threads=new pthread_t[m_thread_number];
	if(!m_threads)
	{
		printf("error2\n");
		throw std::exception();
	}
	for(int i=0;i<thread_number;i++)
	{
		printf("create the %dth thread\n",i);
		if(pthread_create(m_threads+i,NULL,worker,this)!=0)
		{
			delete  [] m_threads;
			throw std::exception();
		}
		if(pthread_detach(m_threads[i]))
		{
			delete[] m_threads;
			throw std::exception();
		}
	}
}
template<typename T>
myserver_threadpool<T>::~myserver_threadpool()
{
	delete[] m_threads;
	m_stop=true;
}

template<typename T>
bool myserver_threadpool<T>::append(T* request)
{
	m_queuelocker.lock();
	if(m_workqueue.size() > m_max_requests)
	{
		m_queuelocker.unlock();
		return false;
	}
	m_workqueue.push_back(request);
	m_queuelocker.unlock();
	m_queuestat.post();
	printf("running append() successfully\n");
	return true;
}

template<typename T>
void* myserver_threadpool<T>::worker(void* arg)
{
	myserver_threadpool* pool=(myserver_threadpool*)arg;
	printf("exec pool->run()\n");
	pool->run();
	printf("finish run()\n");
	return pool;
}

template<typename T>
void myserver_threadpool<T>::run()
{
	while(!m_stop)
	{
		printf("the next running is wait()\n");
		m_queuestat.wait();
		printf("finish wait()\n");
		m_queuelocker.lock();
		if(m_workqueue.empty())
		{
			m_queuelocker.unlock();
			continue;
		}
		T* request = m_workqueue.front();
		m_workqueue.pop_front();
		m_queuelocker.unlock();
		if(!request)
		{
			continue;
		}
		printf("the next running is process()\n");
		request->process();
		printf("finish process()\n");
	}
}
#endif
