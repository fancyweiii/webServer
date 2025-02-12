#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <assert.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include "../log/log.h"
#include "../http/http_conn.h"

using namespace std;

class util_timer;

struct client_data {
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};


class util_timer {
    public:
    util_timer() : prev(nullptr), next(nullptr) {};

    time_t expire;
    client_data *c_data;
    util_timer *prev;
    util_timer *next;

    void (* cb_func)(client_data *);   // 从epoll中删除sockfd
};

class sort_timer_lst {
    private:
    util_timer *head;
    util_timer *tail;

    void add_timer(util_timer *timer, util_timer *lst_head);

    public:
    sort_timer_lst() : head(nullptr), tail(nullptr) {};
    ~sort_timer_lst() {
        util_timer *tmp = head;
        while (tmp != nullptr) {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

};

class Utils {
    public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);
    sort_timer_lst m_timer_lst;
    static int *u_pipefd;  // 与主进程通信管道
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);


#endif