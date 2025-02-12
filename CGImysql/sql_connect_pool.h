#ifndef SQL_CONNECT_POOL_H
#define SQL_CONNECT_POOL_H

#include <mysql/mysql.h>
#include <deque>
#include <iostream>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connect_pool {
    private:
    connect_pool();
    
    ~connect_pool();

    // 最大连接数
    int m_maxconn;
    // 已使用连接数
    int m_curconn;
    // 空闲连接数
    int m_freeconn;
    deque<MYSQL *> connList;
    sem reverse;
    locker lock;

    public:
    string m_url;
    int m_port;
    string m_user;
    string m_password;
    string m_database;
    int m_close_log;

    void init(string url, int port, string user, string passwd, string database, int maxConn, int close_log);
    
    MYSQL *getConnection();      // 数据库连接
    
    bool releaseConnection(MYSQL *conn);        // 放回连接池，不是释放连接
    
    int getFreeConnection();     // 获取当前空闲连接数
    
    void destoryPool();
    
    static connect_pool *getInstance();       // 单例模式

};

/*
？
资源获取即初始化
封装获取连接和释放连接两个操作
即局部申请资源然后自动调用析构函数释放资源
*/
class connectRAII {
    private:
    connect_pool *poolRAII;
    MYSQL *connRAII;

    public:
    connectRAII(connect_pool *pool, MYSQL **conn);
    ~connectRAII();
};

#endif