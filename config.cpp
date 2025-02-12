#include "config.h"

Config::Config() {
    port = 8080;
    // 日志写入默认同步
    logWrite = 0;
    // 触发组合模式，默认listen LT + connfd LT
    trigMode = 0;
    // listen LT
    listenTrigMode = 0;
    // connfd LT
    connTrigMode = 0;
    // 优雅关闭？ 默认关闭
    optLinger = 0;
    // sql连接池数量
    sqlNum = 8;
    // 线程池数量
    threadNum = 8;
    // 默认不关闭日志
    close_log = 0;
    // 并发模型默认proactor
    actorModel = 0;
}

void Config::parseConfig(int argc, char *argv[]) {
    int opt;
    const char* optStr = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, optStr)) != -1) {
        switch (opt)
        {
        case 'p': {
            port = atoi(optarg);
            break;
        }
        case 'l': {
            logWrite = atoi(optarg);
            break;
        }
        case 'm': {
            trigMode = atoi(optarg);
            break;
        }
        case 'o': {
            optLinger = atoi(optarg);
            break;
        }
        case 's': {
            sqlNum = atoi(optarg);
            break;
        }
        case 't': {
            threadNum = atoi(optarg);
            break;
        }
        case 'c': {
            close_log = atoi(optarg);
            break;
        }
        case 'a': {
            actorModel = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}