#include "config.h"

int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "123456";
    string databasename = "yourdb";

    //命令行解析
    Config config;
    config.parseConfig(argc, argv);

    Webserver server;

    //初始化
    server.init(config.port, user, passwd, databasename, config.logWrite, 
                config.optLinger, config.trigMode,  config.sqlNum,  config.threadNum, 
                config.close_log, config.actorModel);
    

    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}