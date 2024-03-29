#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <exception>
#include <stdio.h>
#include <pthread.h>
#include "../lock/locker.h"
#include "../db/connect_pool.h"

template <typename T>
class threadPool
{
public:
    threadPool(int actor_model, connectPool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadPool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    // 工作线程运行的函数, 它不断从工作队列中取出任务并执行
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        // 线程池中的线程数
    int m_max_requests;         // 请求队列中允许的最大请求数
    pthread_t *m_threads;       // 描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; // 请求队列
    locker m_queuelocker;       // 保护请求队列的互斥锁
    sem m_queuestat;            // 是否有任务需要处理
    connectPool *m_conn_pool;   // 数据库
    int m_actor_model;          // 模型切换
};

template <typename T>
threadPool<T>::threadPool(int actor_model, connectPool *conn_pool, int thread_number, int max_requests) : m_actor_model(actor_model), m_thread_number(thread_number),
                                                                                                          m_max_requests(max_requests), m_threads(NULL), m_conn_pool(conn_pool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    // 创建thread_number 个线程, 并将它们设置为detach模式
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadPool<T>::~threadPool()
{
    delete[] m_threads;
}

template <typename T>
bool threadPool<T>::append(T *request, int state)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.emplace_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
bool threadPool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void *threadPool<T>::worker(void *arg)
{
    threadPool *pool = (threadPool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadPool<T>::run()
{
    while (true)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
            continue;
        // 选择Reactor模式
        if (m_actor_model == 1)
        {
            if (request->m_state == 0) // 读
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_conn_pool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        // 选择Proactor模式
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_conn_pool);
            request->process();
        }
    }
}
#endif