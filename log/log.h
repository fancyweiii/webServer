#ifndef LOG_H
#define LOG_H

#include "block_queue.h"
#include <iostream>
#include <fstream>
#include <stdarg.h>
using namespace std;

class Log {
    private:
    string m_dir;
    string m_log_file;
    int m_split_lines;  // 日志最大行数
    int m_day;
    long long m_line_cnt;  // 行数记录
    ofstream log_fp;  // 日志文件fp
    // string m_buf;
    block_queue<string> *m_bp;
    locker m_mutex;
    bool async;  // 同/异步
    int m_close_log;

    Log();
    // 虚构造函数，log的派生类在调用析构函数时不会调用~log()
    virtual ~Log();

    void *async_write_log() {
        string tmp_log;
        // 从阻塞队列中取出一段log写入文件
        while (m_bp->pop(tmp_log)) {
            m_mutex.lock();
            log_fp << tmp_log << endl;
            m_mutex.unlock();
        }
    }

    public:
    static Log *get_instance() {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args) {
        Log::get_instance()->async_write_log();
    }

    bool init(string file_name, int close_log, int split_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, string log_str);

    void flush();
};

#define LOG_DEBUG(log_str) if(0 == m_close_log) {Log::get_instance()->write_log(0, log_str); Log::get_instance()->flush();}
#define LOG_INFO(log_str) if(0 == m_close_log) {Log::get_instance()->write_log(1, log_str); Log::get_instance()->flush();}
#define LOG_WARN(log_str) if(0 == m_close_log) {Log::get_instance()->write_log(2, log_str); Log::get_instance()->flush();}
#define LOG_ERROR(log_str) if(0 == m_close_log) {Log::get_instance()->write_log(3, log_str); Log::get_instance()->flush();}


#endif