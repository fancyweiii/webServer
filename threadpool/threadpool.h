#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <exception>
#include <deque>
#include <vector>
#include "../lock/locker.h"
#include "../log/log.h"
#include "../CGImysql/sql_connect_pool.h"

using namespace std;

template <typename T>
class Threadpool {
    private:
    int m_threads_num;
    int m_max_requests;
    pthread_t *m_threads;
    deque<T *> m_workqueue;
    locker m_mutex;
    sem m_queuestat;
    connect_pool *m_sqlpool;
    int m_actor_model;

    static void *worker(void *args) {
        Threadpool *pool = (Threadpool *)args;
        pool->run();
        return pool;
    }

    void run() {
        while (true) {
            // 等待线程资源
            m_queuestat.wait();
            m_mutex.lock();
            if (m_workqueue.empty()) {
                m_mutex.unlock();
                continue;
            }
            T *request = m_workqueue.front();
            m_workqueue.pop_front();
            if (!request) {
                continue;
            }

            if (m_actor_model == 1) {
                


                // 传入的T的结构？？？？？？？？？

                if (0 == request->m_state) {
                    if (request->read_once()) {
                        request->improv = 1;
                        connectRAII mysqlcon(m_sqlpool, &request->mysql);
                        request->process();
                    }
                    else {
                        request->improv = 1;
                        request->timer_flag = 1;
                    }
                } else {
                    if (request->write()) {
                        request->improv = 1;
                    }
                    else {
                        request->improv = 1;
                        request->timer_flag = 1;
                    }
                }
            } else {
                connectRAII mysqlConn(m_sqlpool, &request->mysql);
                request->process();
            }
        }
    }

    public:
    Threadpool(int actor_model, connect_pool *connPool, int thread_number = 8, int max_request = 10000) {
        m_actor_model = actor_model;
        m_sqlpool = connPool;
        m_threads_num = thread_number;
        m_max_requests = max_request;

        if (thread_number <= 0 || max_request <= 0) {
            throw exception();
        }

        for (int i = 0; i < m_threads_num; i++) {
            if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
                delete[] m_threads;
                throw exception();
            }
            // 线程设置为分离状态，终止时自动释放资源
            if (pthread_detach(m_threads[i])) {
                delete[] m_threads;
                throw exception();
            }
        }
    }
    
    ~Threadpool() {
        delete[] m_threads;
    }

    bool append_p(T *request) {
        m_mutex.lock();

        if (m_workqueue.size() >= m_max_requests) {
            m_mutex.unlock();
            return false;
        }
        m_workqueue.push_back(request);
        m_mutex.unlock();
        m_queuestat.post();
        return true;
    }

    bool append(T *request, int stat) {
        m_mutex.lock();

        if (m_workqueue.size() >= m_max_requests) {
            m_mutex.unlock();
            return false;
        }
        request->m_state = stat;
        m_workqueue.push_back(request);
        m_mutex.unlock();
        m_queuestat.post();
        return true;
    }
};


#endif