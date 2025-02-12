#include "log.h"
#include <chrono>
#include <filesystem>
using namespace std;

Log::Log() {
    m_line_cnt = 0;
    async = false;
}

Log::~Log() {
    if (log_fp.is_open()) {
        log_fp.close();
    }
}

bool Log::init(string file_name, int close_log, int split_lines, int max_queue_size) {
    if (max_queue_size > 0) {
        async = true;
        m_bp = new block_queue<string>(max_queue_size);
        pthread_t pid;
        pthread_create(&pid, NULL, flush_log_thread, NULL);
    }

    m_close_log = close_log;
    m_split_lines = split_lines;

    // 获取时间
    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    tm local_time = *localtime(&now_time);
    string log_time = to_string(local_time.tm_year + 1900) + "-" + to_string(local_time.tm_mon + 1) + "-" + to_string(local_time.tm_mday);

    size_t slash = file_name.find_last_of('/');
    string log_loc;
    if (slash != string::npos) {
        m_dir = file_name.substr(0, slash + 1);
        m_log_file = file_name.substr(slash + 1);
        log_loc = m_dir + log_time + m_log_file;
        if (!filesystem::exists(m_dir)) {
            filesystem::create_directories(m_dir);
        }
    } else {
        m_log_file = file_name;
        log_loc = log_time + m_log_file;
    }

    m_day = local_time.tm_mday;

    log_fp.open(log_loc, ios::app);
    if (!log_fp.is_open()) {
        return false;
    }

    return true;
}

void Log::write_log(int level,string log_str) {
    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    tm local_time = *localtime(&now_time);

    string log_to_write;
    switch (level)
    {
    case 0:
        log_to_write = "[DEBUG]:";
        break;
    case 1:
        log_to_write = "[INFO]:";
        break;
    case 2:
        log_to_write = "[WARN]:";
        break;
    case 3:
        log_to_write = "[ERROR]:";
        break;
    default:
        log_to_write = "[INFO]:";
        break;
    }

    m_mutex.lock();

    m_line_cnt++;

    // 判断是否跨天  是否需要翻页
    if (local_time.tm_mday != m_day || m_line_cnt % m_split_lines == 0) {
        log_fp.close();
        string log_time = to_string(local_time.tm_year + 1900) + "-" + to_string(local_time.tm_mon + 1) + "-" + to_string(local_time.tm_mday);
        string new_log;

        if (m_day != local_time.tm_mday) {
            new_log = m_dir + log_time + m_log_file;
            m_day = local_time.tm_mday;
            m_line_cnt = 0;
        } else {
            new_log = m_dir + log_time + "(" + to_string(m_line_cnt / m_split_lines) + ")" + m_log_file;
        }

        log_fp.open(new_log, ios::app);
    }

    m_mutex.unlock();

    log_str = to_string(local_time.tm_year + 1900) + "-" + to_string(local_time.tm_mon + 1) + "-" + to_string(local_time.tm_mday) + " " + to_string(local_time.tm_hour) + ":" + to_string(local_time.tm_min) + ":" + to_string(local_time.tm_sec) + " " + log_to_write + log_str;

    if (async && !m_bp->is_full()) {
        m_bp->push(log_str);
    } else {
        // 同步写
        m_mutex.lock();
        log_fp << log_str << endl;
        m_mutex.unlock();
    }
}

void Log::flush() {
    m_mutex.lock();
    log_fp.flush();
    m_mutex.unlock();
}
