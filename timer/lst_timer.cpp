#include "lst_timer.h"

using namespace std;

void sort_timer_lst::add_timer(util_timer *timer) {
    if (timer == nullptr) {
        return;
    }
    if (head == nullptr) {
        head = timer;
        tail = timer;
        return;
    }
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

// 有数据收发时，调整timer的expire，向后调整，只会增大
void sort_timer_lst::adjust_timer(util_timer *timer) {
    if (timer == nullptr) {
        return;
    }
    util_timer *tmp = timer->next;
    // 在末尾或者延长后仍然小于
    if ((tmp == nullptr) || (timer->expire < tmp->expire)) {
        return;
    }
    if (head == timer) {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    } else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        timer->next = nullptr;
        timer->prev = nullptr;
        add_timer(timer, timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer *timer) {
    if (timer == nullptr) {
        return;
    }

    if ((timer == head) && (timer == tail)) {
        head = nullptr;
        tail = nullptr;
        return;
    }

    if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }

    if (timer == tail) {
        tail = timer->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }

    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

// 处理过期事件
void sort_timer_lst::tick() {
    if (head == nullptr) {
        return;
    }

    time_t cur = time(nullptr);
    util_timer *tmp = head;
    while (tmp != nullptr) {
        if (cur < tmp->expire) {
            break;
        }

        tmp->cb_func(tmp->c_data);
        head = head->next;
        if (head != nullptr) {
            head->prev = nullptr;
        }
        delete tmp;
        tmp = head;
    }
}

// 调用私有add确定head指向的timer的超时时间大于现有timer
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) {
    util_timer *tmp = lst_head;
    while (tmp->next != nullptr) {
        if (timer->expire < tmp->next->expire) {
            timer->next = tmp->next;
            timer->prev = tmp;
            tmp->next = timer;
            timer->next->prev = timer;
            return;
        }
        tmp = tmp->next;
    }
    tmp->next = timer;
    timer->prev = tmp;
    timer->next = nullptr;
    tail = timer;
}

void Utils::init(int timeslot) {
    m_TIMESLOT = timeslot;
}

// 保留原有标志位的同时添加非阻塞特性
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;
    if (TRIGMode == 1) {
        event.events |= EPOLLET;
    }
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::sig_handler(int sig) {
    // 保留errno
    int old_errno = errno;
    int msg = sig;
    // 写入信号
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = old_errno;
}

void Utils::addsig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);  // 信号添加到信号集
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 每slot触发sigalrm信号
void Utils::timer_handler() {
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info) {
    send(connfd, info, sizeof(info), NULL);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;


void cb_func(client_data *user_data) {
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);

    http_conn::m_user_cnt--;
}