#include "webServer.h"

int webServer::m_pipefd[2] = {0, 0};
int webServer::m_TIMESHOT = 5;

void webServer::init(int port, int max_thread_num, int max_request_num, bool isReactor, bool isListenfdET, bool isConnfdET, int TIMESHOT)
{
    m_epollfd = 0;
    m_port = port;
    max_thread_number = max_thread_num;
    max_request_number = max_request_num;
    m_isReactor = isReactor;
    m_isListenfdET = isListenfdET;
    m_isConnfdET = isConnfdET;
    m_TIMESHOT = TIMESHOT;
    users = std::vector<http>(MAX_FD_COUNT);
    user_timers = std::vector<std::shared_ptr<timer>>(MAX_FD_COUNT, nullptr);
    getcwd(m_server_root, 128);
    strcat(m_server_root, "/root");
    http::m_server_root = m_server_root;
    http::isConnfdET = m_isConnfdET;
    http::isReactor = m_isReactor;
}

void webServer::run()
{
    threadpoolInit();
    logInit();
    eventListen();
    eventLoop();
}

void webServer::threadpoolInit()
{
    m_pool = std::shared_ptr<threadpool<http>>(new threadpool<http>(max_thread_number, max_request_number));
}

void webServer::logInit()
{
    Log::get_instance()->init("ServerLog_");
}

void webServer::eventListen()
{
    // 注册listenfd和epollfd
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 设置listen地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    // 设置允许监听套接字复用,可以直接bind，无视端口的TIME_WAIT状态
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    // 绑定listenfd地址
    int ret = 0;
    ret = bind(m_listenfd, (sockaddr *)&address, sizeof(address));
    assert(ret >= 0);

    // 开始监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // 创建epollfd
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    http::m_epollfd = m_epollfd;
    // 设置定时器
    m_lst_timer.m_epollfd = m_epollfd;
    alarm(m_TIMESHOT);

    // 添加listenfd进入事件表，同时设为非阻塞
    // ET模式+非ONESHOT
    http::addfd(m_epollfd, m_listenfd, false, m_isListenfdET);

    // 创建管道以处理信号
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    http::setnonblock(m_pipefd[1]);
    http::addfd(m_epollfd, m_pipefd[0], false, 0);

    // 绑定信号处理函数
    addsig(SIGPIPE, SIG_IGN, true);      // 忽略SIGPIPE信号
    addsig(SIGALRM, sig_handler, false); // 定时器心跳信号
    addsig(SIGTERM, sig_handler, false); // 停止信号

    LOG_INFO("start eventListen")
}

void webServer::eventLoop()
{
    // 开始epoll wait循环
    LOG_INFO("start eventLoop");
    stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            // 信号处理程序中断了
            break;
        }

        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == m_listenfd)
            {
                // 处理新用户连接
                dealNewClient();
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务端关闭连接，移除对应的定时器
                // 等待定时器自动关闭
                LOG_DEBUG("server close fd postive %d", sockfd);
                m_lst_timer.deal_timer(user_timers[sockfd]);
            }
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                // 处理管道发送来的信号
                dealWithSignal();
            }
            else if (events[i].events & EPOLLIN)
            {
                // sockfd上有可读事件
                dealWithRead(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                // 将sockfd对应的连接的写缓冲区内容send出去
                dealWithWrite(sockfd);
            }
        }
    }
}

bool webServer::dealNewClient()
{
    if (m_isListenfdET)
    {
        // ET listen
        while (true)
        {
            if (!dealNewClientCore())
            {
                break;
            }
        }
    }
    else
    {
        // LT listen
        dealNewClientCore();
    }

    return true;
}

bool webServer::dealNewClientCore()
{
    // true 正常添加完
    // false 连接上限/无新连接
    std::shared_ptr<sockaddr_in> client_address = std::make_shared<sockaddr_in>();
    socklen_t client_addr_length = sizeof(sockaddr_in);
    int connfd = accept(m_listenfd, (struct sockaddr *)client_address.get(), &client_addr_length);
    if (connfd < 0)
    {
        // 无新的连接到来
        return false;
    }
    if (m_user_count >= MAX_FD_COUNT)
    {
        // 连接数已上限
        return false;
    }
    // 初始化connfd并添加epoll监听
    users[connfd].init(connfd, client_address);
    http::addfd(m_epollfd, connfd, true, m_isConnfdET);
    LOG_DEBUG("get new client, fd %d", connfd);
    ++m_user_count;

    // 记录对端IP
    char clientIP[64];
    inet_ntop(AF_INET, &client_address->sin_addr.s_addr, clientIP, 64);

    // 添加定时器
    std::shared_ptr<timer> t_timer(new timer);
    t_timer->sockfd = connfd;
    t_timer->expire_time = time(NULL) + m_TIMESHOT;
    m_lst_timer.add_timer(t_timer);
    user_timers[connfd] = t_timer;
    return true;
}

// 处理管道发送来的信号
bool webServer::dealWithSignal()
{
    // 从管道中获取信号
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                m_lst_timer.tick();
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                LOG_INFO("get signal SIGTERM");
                LOG_INFO("webserver stop");
                break;
            }
            }
        }
    }
    return true;
}

// 处理connfdfd上的可读事件
bool webServer::dealWithRead(int connfd)
{
    // 1. Reactor，交由线程池进行读-处理
    // 2. Proactor，主线程读完后交由线程池进行处理
    // 更新定时器
    user_timers[connfd]->delay_time(m_TIMESHOT);
    m_lst_timer.adjust_timer(user_timers[connfd]);

    bool isReactor = true;
    if (isReactor)
    {
        // Reactor模式
        // 交由线程池进行读和处理
        users[connfd].m_state = 0;
        m_pool->append(&users[connfd]);
    }
    else
    {
        // Proactor模式
        // 在该线程中进行读，在线程池中进行处理
        if (users[connfd].read_once())
        {
            users[connfd].m_state = 0;
            m_pool->append(&users[connfd]);
        }
    }
    return false;
}

// 将connfd写缓冲区中内容send出去
bool webServer::dealWithWrite(int connfd)
{
    // 更新定时器
    user_timers[connfd]->delay_time(m_TIMESHOT);
    m_lst_timer.adjust_timer(user_timers[connfd]);

    bool isReactor = true;
    if (isReactor)
    {
        // Reactor模式
        // 交由线程池进行发送
        users[connfd].m_state = 1;
        m_pool->append(&users[connfd]);
    }
    else
    {
        // Proactor模式
        // 主线程进行发送
        if (users[connfd].write_once())
        {
            // 更新定时器
            users[connfd].m_state = 1;
        }
    }
    return false;
}

void webServer::sig_handler(int sig)
{
    // 为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(m_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void webServer::addsig(int sig, void(hander_func)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = hander_func;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
