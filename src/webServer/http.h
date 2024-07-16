#pragma once

// 可交由threadpool执行的任务
// 处理HTTP请求
class http
{
public:
    http();
    ~http();

    // 重置为初始状态
    void init();

    // 交由线程池处理的函数，入口函数
    void process();

    // 解析请求
    void parseRequest();

    // 解析一行内容
    void parseLine();

    // 分割一行
    void getLine();

private:
    static int m_epollfd; // 属于的epoll监听
    int m_connfd;         // 对端的socketfd
};