#pragma once

#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <string.h>

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init();

    // 对文件描述符设置非阻塞
    static int setnonblocking(int fd);

    // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    static void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    // 设置信号函数
    static void addsig(int sig, void (*handler)(int), bool restart = true);

    // 向socket发送消息并关闭socket
    static void show_error(int sockfd, const char *info);

public:
    static int *u_pipefd;
    static int u_epollfd;
};