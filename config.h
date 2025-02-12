#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

using namespace std;

class Config {
public:
    Config();
    ~Config() {};

    void parseConfig(int argc, char *argv[]);

    int port;
    // 日志写入方式
    int logWrite;
    // ？触发组合模式
    int trigMode;
    // ？listenfd写入方式
    int listenTrigMode;
    // ？connfd触发模式
    int connTrigMode;

    // ?? 优雅触发模式
    int optLinger;

    int sqlNum;

    int threadNum;

    int close_log;
    // 并发模型
    int actorModel;
};

#endif
