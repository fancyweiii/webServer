#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <stdlib.h>
#include <sys/time.h>
#include "../lock/locker.h"

// next = (back + 1) % max_size
// 循环数组实现队列
template <class T>
class block_queue {
    private:
    locker m_mutex;
    cond m_cond;
    int m_max_size;
    int m_size;
    int m_front;
    int m_back;
    T *m_queue;

    public:
    block_queue(int max_size = 1000) {
        if (max_size <= 0) {
            exit(1);
        }

        m_mutex.lock();
        m_max_size = max_size;
        m_queue = new T[max_size];
        m_size = 0;
        m_front = 0;
        m_back = -1;
        m_mutex.unlock();
    }

    ~block_queue() {
        m_mutex.lock();

        if (m_queue != nullptr) {
            delete [] m_queue;
        }

        m_mutex.unlock();
    }

    void clear() {
        m_mutex.lock();
        m_size = 0;
        m_front = 0;
        m_back = -1;
        m_mutex.unlock();
    }

    bool is_empty() {
        m_mutex.lock();

        if (m_size == 0) {
            m_mutex.unlock();
            return true;
        }

        m_mutex.unlock();
        return false;
    }

    bool is_full() {
        m_mutex.lock();

        if (m_size >= m_max_size) {
            m_mutex.unlock();
            return true;
        }

        m_mutex.unlock();
        return false;
    }

    bool front(T &item) {
        m_mutex.lock();

        if (m_size <= 0) {
            m_mutex.unlock();
            return false;
        }

        item = m_queue[m_front];
        m_mutex.unlock();
        return true;
    }

    bool back(T &item) {
        m_mutex.lock();

        if (m_size <= 0) {
            m_mutex.unlock();
            return false;
        }

        item = m_queue[m_back];
        m_mutex.unlock();
        return true;
    }

    int size() {
        m_mutex.lock();
        int temp = m_size;
        m_mutex.unlock();
        return temp;
    }

    int max_size() {
        m_mutex.lock();
        int temp = m_max_size;
        m_mutex.unlock();
        return temp;
    }

    bool push(T &item) {
        m_mutex.lock();

        if (m_size >= m_max_size) {
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_size++;
        m_queue[m_back] = item;
        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    //没有元素时阻塞进程
    bool pop(T &item) {
        m_mutex.lock();

        // 持续阻塞直至唤醒或者唤醒失败
        while (m_size <= 0) {
            // 线程唤醒失败
            if (!m_cond.wait(m_mutex.get())) {
                m_mutex.unlock();
                return false;
            }
        }

        item = m_queue[m_front];
        m_front = (m_front + 1) % m_max_size;
        m_size--;
        m_mutex.unlock();
        return true;
    }

    bool pop(T &item, int timeout) {
        m_mutex.lock();

        struct timeval now = {0, 0};
        struct timespec tmp = {0, 0};
        gettimeofday(&now, NULL);
        tmp.tv_sec = now.tv_sec + timeout / 1000;
        tmp.tv_nsec = now.tv_usec    + (timeout % 1000) * 1000;
        // 定时阻塞
        if (m_size <= 0 && (!m_cond.timewait(m_mutex.get(), tmp))) {
            m_mutex.unlock();
            return false;
        }

        item = m_queue[m_front];
        m_front = (m_front + 1) % m_max_size;
        m_size--;
        m_mutex.unlock();
        return true;
    }
    
};

#endif