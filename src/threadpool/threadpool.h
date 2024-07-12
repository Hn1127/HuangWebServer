#pragma once

#include <pthread.h>
#include <vector>
#include <list>
#include "../locker/lock.h"

// 创建线程池以处理 T 的任务
template <class T>
class threadpool
{
public:
    threadpool(int max_thread_num = 8, int max_request_num = 1000);
    ~threadpool();
    // 向工作队列中添加一个任务，线程将从队列中取出并处理
    bool append(T *request, int state);

private:
    // 线程入口函数，创建线程时进入该函数
    static void *worker(void *arg);
    // 线程实际的运行函数，不断从队列中取出request进行处理
    void run();

private:
    int max_thread_number;      // 最大线程数
    int max_request_number;     // 队列中最大任务数
    pthread_t *m_threads;       // 线程数组
    std::list<T *> m_workqueue; // 工作队列
    MutexLock m_mutex;          // 访问工作队列的互斥锁
    sem m_sem;                  // 查看工作队列是否有工作的信号量
};

template <class T>
threadpool<T>::threadpool(int max_thread_num, int max_request_num)
    : max_thread_number(max_thread_num), max_request_number(max_request_num)
{
    // 初始化线程
    if (max_thread_num <= 0 || max_request_num <= 0)
        throw std::exception();
    m_threads = new pthread_t[max_thread_number];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < max_thread_number; ++i)
    {
        // 初始化线程进入worker函数
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            std::cout << "threadpool failed!\n";
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            std::cout << "threadpool failed!\n";
            throw std::exception();
        }
    }
}

template <class T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template <class T>
bool threadpool<T>::append(T *request, int state)
{
    // state=0为读事件，state=1为写事件
    // 向队列中添加任务
    {
        MutexLockGuard m(this->m_mutex);
        if (m_workqueue.size() >= max_request_number)
        {
            return false;
        }
        m_workqueue.push_back(request);
        m_sem.post();
    }
    return true;
}

template <class T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <class T>
void threadpool<T>::run()
{
    // 线程工作在该线程，从队列中取出一项任务进行处理
    while (true)
    {
        // 等待任务到达
        m_sem.wait();
        // 取出一项任务
        T *request = nullptr;
        {
            MutexLockGuard m(this->m_mutex);
            if (m_workqueue.empty())
            {
                continue;
            }
            request = m_workqueue.front();
            m_workqueue.pop_front();
        }
        // 判断request的有效性
        if (!request)
        {
            continue;
        }
        // 处理任务
        request->process();
    }
}