#pragma once

#include "log.h"
#include <sys/epoll.h>
#include <memory>
#include <unistd.h>
#include <stdio.h>

// 定时器
class timer
{
public:
    void delay_time(int TIMESHOT);

public:
    time_t expire_time; // 定时器到的时间
    int sockfd;
    std::shared_ptr<timer> prev;
    std::shared_ptr<timer> next;
};

// 定时器链表
class lst_timer
{
public:
    lst_timer();

    // 初始化定时器链表
    lst_timer(int epollfd);

    // 进行一次心跳检测，设置ALARM信号
    void tick();

    // 添加定时器
    void add_timer(std::shared_ptr<timer> timer);

    // 调整指定定时器
    void adjust_timer(std::shared_ptr<timer> timer);

    // 删除定时器，关闭对应socketfd
    void deal_timer(std::shared_ptr<timer> timer);

public:
    int m_epollfd;
    int m_TIMESHOT;

private:
    std::shared_ptr<timer> head;
    std::shared_ptr<timer> tail;
};