#pragma once

#include "handler.h"
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

const int READ_BUFFER_SIZE = 1024;
const int WRITE_BUFFER_SIZE = 1024;

// 可交由 threadpool 执行的任务
class echo : public handler
{
public:
    void init(int connfd, const sockaddr_in &addr);

    void init();

    void process();

    bool read_once();

    void flush_file(const char *filename, char *msg, size_t size);

public:
    static int m_epollfd;
    static int m_user_count;

private:
    int m_connfd;
    sockaddr_in m_addr;

    // 读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];
    // 标识读缓冲中已经读入的的客户数据的最后一个自己的下一位置
    int m_read_idx;

    // 写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
    // 写缓冲区中待发送的字节数
    int m_write_idx;
};

// 对文件描述符设置非阻塞
int setnonblocking(int fd);

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
// 从内核时间表删除描述符
void removefd(int epollfd, int fd);

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode);