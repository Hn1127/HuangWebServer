#pragma once

#include "lst_timer.h"
#include "log.h"
#include "sqlConnectionPool.h"
#include <memory>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <arpa/inet.h>

// 可交由threadpool执行的任务
// 处理HTTP请求
class http
{
public:
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    static const int FILENAME_LEN = 200;
    static int m_epollfd;       // 属于的epoll监听
    static bool isReactor;      // Reactor模式
    static bool isConnfdET;     // ET触发方式
    static char *m_server_root; // 服务器运行根目录
    enum METHOD
    {
        GET = 0,
        POST
    };
    // 当前解析请求的解析状态
    enum PARSE_STATUS
    {
        PARSE_REQUESTLINE = 0, // 查看请求行
        PARSE_HEADER,          // 查看请求头
        PARSE_CONTENT          // 查看请求体
    };
    // 请求的具体状态
    enum HTTP_CODE
    {
        NO_REQUEST = 0, // 不是完整的请求
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    // 解析行状态
    enum LINE_STATE
    {
        LINE_OK = 0, // 检查完一行
        LINE_BAD,    // 检查完一行，但该行有误
        LINE_OPEN    // 该行未完
    };

public:
    http();
    ~http();

    // 初始化
    void init(int connfd, std::shared_ptr<sockaddr_in> addr, sqlConnectionPool *sqlPool);

    // 重置为初始状态
    void init();

    // 交由线程池处理的函数，入口函数
    void process();

    // 接收到EPOLLIN信号后进行读取
    bool read_once();

    // 将缓冲区中内容发送
    bool write_once();

    // 获得下一行
    LINE_STATE getLine(char *&curLine);

    // 将文件描述符设为非阻塞
    static int setnonblock(int fd);

    // 注册socket事件监听，并设为非阻塞，选择是否为ONESHOT和ET模式
    static void addfd(int epollfd, int fd, bool ONESHOT = true, bool isETTRIG = true);

    // 为文件描述符更改监听事件，选择是否重置ONESHOT
    static void modfd(int epollfd, int fd, int event, bool ONESHOT = true, bool isETTRIG = true);

    // 删除事件监听
    static void removefd(int epollfd, int fd);

private:
    // 分割一行内容
    LINE_STATE checkLine();

    // 解析请求入口
    HTTP_CODE process_request();

    // 解析请求行
    HTTP_CODE parse_request_line(char *curLine);

    // 解析请求头
    HTTP_CODE parse_request_header(char *curLine);

    // 解析请求体
    HTTP_CODE parse_request_content(char *curLine);

    // 解析请求
    HTTP_CODE parse_request();

    // 填写响应
    bool process_response(HTTP_CODE request_code);

    // 往写缓冲区中写内容
    bool add_response(const char *format, ...);

    // 添加响应行
    bool add_response_line(int status, const char *title);

    // 添加响应消息报头
    bool add_response_header(int content_len);

    // 添加响应正文
    bool add_response_content(const char *content);

    void unmap();

public:
    int m_state;  // 0读状态，1写状态
    int m_connfd; // 对端的socketfd
    sqlConnectionPool *m_sqlPool;

private:
    // socket信息
    std::shared_ptr<sockaddr_in> m_addr; // 对端的address

    // 读写信息
    char m_read_buf[READ_BUFFER_SIZE];   // 读缓冲区
    long m_read_idx;                     // 读缓冲区末尾位置
    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
    long m_write_idx;                    // 写缓冲区末尾位置

    // 解析信息
    long m_parse_idx;            // 解析的当前位置
    long m_line_start;           // 当前行的起始位置
    PARSE_STATUS m_parse_status; // 当前的解析状态
    char *m_url;                 // 请求的URL
    METHOD m_method;             // 请求方法 GET/POST
    char *m_version;             // HTTP版本
    long m_content_length;       // 请求体的长度
    bool m_linger;               // 是否为keep-alive
    char *m_host;                // HTTP请求的HOST,即本server的运行IP:PORT
    char *m_content;             // 请求体,在本服务器中为用户名&密码

    // 响应信息
    char m_filename[256];     // 响应请求的正文对应文件
    struct stat m_file_state; // 请求文件stat
    char *m_file_address;     // 文件使用map映射到的地址
    int m_bytes_to_send;      // 将要发送的字节数
    int m_bytes_have_send;    // 已经发送的字节数

    struct iovec m_iv[2]; // 集中写
    int m_iv_count;
};