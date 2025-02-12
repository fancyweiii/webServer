#include "sql_connect_pool.h"

connect_pool::connect_pool() {
    m_curconn = 0;
    m_freeconn = 0;
}

connect_pool::~connect_pool() {
    destoryPool();
}

void connect_pool::init(string url, int port, string user, string passwd, string database, int maxConn, int close_log) {
    m_url = url;
    m_port = port;
    m_user = user;
    m_password = passwd;
    m_close_log = close_log;

    for (int i = 0; i < maxConn; i++) {
        MYSQL *conn = NULL;
        conn = mysql_init(conn);

        if (conn == NULL) {
            LOG_ERROR("MYSQL INIT ERROR");
            exit(1);
        }

        conn = mysql_real_connect(conn, url.c_str(), user.c_str(), passwd.c_str(), database.c_str(), port, NULL, 0);

        if (conn == NULL) {
            LOG_ERROR("MYSQL CONNECT ERROR");
            exit(1);
        }

        connList.push_back(conn);
        m_freeconn++;
    }

    // 信号量表示空闲连接数量
    reverse = sem(m_freeconn);
    m_maxconn = m_freeconn;
}

MYSQL *connect_pool::getConnection() {
    MYSQL *conn;

    if (connList.size() == 0) {
        return NULL;
    }

    reverse.wait();
    lock.lock();

    conn = connList.front();
    connList.pop_front();

    m_curconn++;
    m_freeconn--;

    lock.unlock();
    return conn;
}

bool connect_pool::releaseConnection(MYSQL *conn) {
    if (conn == NULL) {
        return false;
    }

    lock.lock();

    connList.push_back(conn);
    m_curconn--;
    m_freeconn++;

    lock.unlock();
    reverse.post();

    return true;
}

int connect_pool::getFreeConnection() {
    return m_freeconn;
}

void connect_pool::destoryPool() {
    lock.lock();

    for (auto ms : connList) {
        mysql_close(ms);
    }

    m_freeconn = 0;
    m_curconn = 0;

    lock.unlock();
}

connect_pool *connect_pool::getInstance() {
    static connect_pool conn;
    return &conn;
}

connectRAII::connectRAII(connect_pool *pool, MYSQL **conn) {
    *conn = pool->getConnection();
    connRAII = *conn;
    poolRAII = pool;
}

connectRAII::~connectRAII() {
    poolRAII->releaseConnection(connRAII);
}