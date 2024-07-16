#pragma once

#include <memory>
#include <vector>
#include "http.h"
#include "threadpool.h"

// 接收并处理HTTP请求
class webServer
{
public:
    // 初始化参数
    void init();

    // 启动服务器，开启事件监听和epoll_wait循环
    void run();

    // 初始化线程池
    void threadpollInit();

    // 开启监听
    void eventListen();

    // 开始循环
    void eventLoop();

private:
    // 将文件描述符设为非阻塞
    static void setnonblock(int fd);

    // 注册socket事件监听，并设为非阻塞，选择是否为ONESHOT和ET模式
    static void addfd(int epollfd, int fd, bool ONESHOT = true, bool isETTRIG = true);

    // 为文件描述符更改监听事件，选择是否重置ONESHOT
    static void modfd(int epollfd, int fd, int event, bool ONESHOT = true, bool isETTRIG = true);

    // 删除事件监听
    static void removefd(int epollfd, int fd);

private:
    int epollfd;

    // 线程池
    std::shared_ptr<threadpool<http>> m_pool;
    // 用户连接
    std::vector<std::shared_ptr<http>> users; // 每个用户根据socketfd，从列表中获取对应的连接
};