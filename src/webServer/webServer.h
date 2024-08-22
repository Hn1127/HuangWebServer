#pragma once

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <arpa/inet.h>
#include <memory>
#include <vector>
#include <signal.h>
#include <assert.h>
#include "http.h"
#include "threadpool.h"
#include "log.h"

// 接收并处理HTTP请求
class webServer
{
public:
    // 初始化参数
    void init(int port = 9000, int max_thread_num = 8, int max_request_number = 1000, bool isReactor = true, bool isListenfdET = true, bool isConnfdET = true);

    // 启动服务器，开启事件监听和epoll_wait循环
    void run();

    // 初始化线程池
    void threadpoolInit();

    // 初始化日志
    void logInit();

    // 开启监听
    void eventListen();

    // 开始循环
    void eventLoop();

private:
    // 处理新用户连接
    bool dealNewClient();

    // 处理新用户连接核心代码
    bool dealNewClientCore();

    // 处理信号
    bool dealWithSignal();

    // 处理读事件
    bool dealWithRead(int connfd);

    // 处理写事件
    bool dealWithWrite(int connfd);

    // 处理信号函数
    static void sig_handler(int sig);

    // 添加信号
    static void addsig(int sig, void(hander_func)(int), bool restart = true);

private:
    // 基本信息,在init时确定
    int m_port;              // 运行端口
    int max_thread_number;   // 线程池大小
    int max_request_number;  // 最大可接收请求数，实为线程池最大可接收任务数
    bool m_isReactor;        // Reactor和Proactor,将同步给http
    bool m_isListenfdET;     // m_listenfd是否为ET触发
    bool m_isConnfdET;       // 客户端连接connfd是否为ET触发
    char m_server_root[128]; // 服务器运行的根目录

    bool stop_server;
    bool timeout; // 定时器时间到将进行一次心跳检测

    // 文件描述符
    int m_epollfd;
    int m_listenfd;
    static int m_pipefd[2]; // 管道，用于同一事件源

    static const int MAX_EVENT_NUMBER = 10000;
    static const int MAX_FD_COUNT = 65536;

    // epoll任务
    epoll_event events[MAX_EVENT_NUMBER];

    // 线程池
    std::shared_ptr<threadpool<http>> m_pool;
    // 用户连接
    std::vector<http> users; // 每个用户根据socketfd，从列表中获取对应的连接
    int m_user_count;        // 当前连接数
};