#include "echo.h"

int echo::m_user_count = 0;

// 对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void echo::init(int connfd, const sockaddr_in &addr)
{
    m_connfd = connfd;
    m_addr = addr;
    // 添加epoll监听并设为非阻塞
    addfd(m_epollfd, m_connfd, true, 1);
    ++m_user_count;
    init();
}

void echo::init()
{
    // 初始化状态
    m_read_idx = 0;
    m_write_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
}

void echo::process()
{
    // 将读缓冲区中的内容写入发送缓冲区
    memcpy(m_write_buf, m_read_buf, READ_BUFFER_SIZE);
    int bytes_to_send = m_read_idx;
    int write_bytes = 0;
    m_write_idx = 0;
    // 将读缓冲区内容先写入日志文件
    LOG_INFO(m_read_buf);

    while (true)
    {
        // 循环发送
        write_bytes = send(m_connfd, m_write_buf + m_write_idx, bytes_to_send, 0);
        if (write_bytes == -1)
        {
            // 读取终止
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            return;
        }
        else if (write_bytes == 0)
        {
            // 对端关闭
            return;
        }
        m_write_idx += write_bytes;
        bytes_to_send -= write_bytes;
        if (bytes_to_send == 0)
        {
            modfd(m_epollfd, m_connfd, EPOLLIN, 1);
            init();
            return;
        }
    }
}

bool echo::read_once()
{
    // 非阻塞读取数据到缓冲区中
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        // 缓冲区已满
        return false;
    }
    int bytes_read = 0;
    while (true)
    {
        // 从socket读写数据，最大数量为读缓冲区剩余容量
        bytes_read = recv(m_connfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            // 读取终止
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                break;
            }
            return false;
        }
        else if (bytes_read == 0)
        {
            // 对方已断开
            return true;
        }
        m_read_idx += bytes_read;
    }
    return true;
}
