#pragma once

#include <sys/epoll.h>
#include <sys/socket.h>
#include <assert.h>
#include <memory>
#include <vector>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include "echo.h"
#include "threadpool.h"

const int MAX_EVENT_NUMBER = 10000; // epoll event 数目
const int MAX_FD_NUMBER = 65536;    // 文件描述符数目，即最多处理的连接数

// 监听端口，将接收到的信息原路返回
class echoServer
{
public:
    echoServer();
    ~echoServer();

public:
    void init(int port = 9000, int thread_num = 8);
    void threadpoolInit();
    void run();
    void eventListen();
    void eventLoop();

private:
    // 基本信息
    int m_port; // 监听端口

    // 任务
    std::vector<echo> echos;

    // 线程池相关
    std::shared_ptr<threadpool<echo>> m_pool;
    int max_thread_num; // 线程池线程数

    // epoll event相关
    int m_epollfd;                        // epoll
    epoll_event events[MAX_EVENT_NUMBER]; // 监听数

    // 文件描述符
    int m_pipefd[2]; // 接收信号处理,统一事件源
    int m_listenfd;  // 监听文件描述符
};
