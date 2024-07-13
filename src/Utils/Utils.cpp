#include "Utils.h"

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

// 初始化
void Utils::init()
{
    // donothing
}

int Utils::setnonblocking(int fd)
{
    // 将fd设置为非阻塞的fd
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    // 向epoolfd中添加fd
    epoll_event event;
    event.data.fd = fd;

    // 设置ET和LT
    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    // 设置oneshot
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::sig_handler(int sig)
{
    // 处理信号，将信号写入管道交由管道另一端处理，统一事件源
    // 为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    // 注册信号，设置处理函数
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    // 使得处理完信号后能继续执行原本在执行的阻塞系统调用
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::show_error(int sockfd, const char *info)
{
    // 向socket发送错误提示信息，关闭socket
    send(sockfd, info, sizeof(info), 0);
    close(sockfd);
}
