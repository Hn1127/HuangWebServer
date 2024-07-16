#include "echoServer.h"

int echo::m_epollfd = 0;

echoServer::echoServer()
{
    m_port = 9000;
    max_thread_num = 8;
    m_epollfd = 0;
}

echoServer::~echoServer()
{
    close(m_epollfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    close(m_listenfd);
}

void echoServer::init(int port, int thread_num)
{
    m_port = port;
    max_thread_num = thread_num;
    echos = std::vector<std::shared_ptr<echo>>(MAX_FD_NUMBER, std::shared_ptr<echo>(new echo));
}

void echoServer::threadpoolInit()
{
    m_pool = std::shared_ptr<threadpool<echo>>(new threadpool<echo>(max_thread_num));
}

void echoServer::run()
{
    threadpoolInit();
    eventListen();
    eventLoop();
}

void echoServer::eventListen()
{
    // 初始化epollfd和listenfd，注册入epoll时间表开始监听
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 创建套接字
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    // 设置允许监听套接字复用，可以直接bind，无视端口的TIME_WAIT状态
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    // 绑定listenfd到指定地址
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);

    // 开始监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // 创建epoolfd
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    // 添加listenfd开始监听，设为非阻塞
    addfd(m_epollfd, m_listenfd, false, 1);
    echo::m_epollfd = m_epollfd;

    // 创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    setnonblocking(m_pipefd[1]);
    addfd(m_epollfd, m_pipefd[0], false, 0);
}

void echoServer::eventLoop()
{
    // 开始epoll循环
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            // 监听出问题
            break;
        }

        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == m_listenfd)
            {
                // 处理新客户连接
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (connfd < 0)
                {
                    // 连接有问题
                    continue;
                }
                if (echo::m_user_count >= MAX_FD_NUMBER)
                {
                    Utils::show_error(connfd, "Internal server busy");
                    break;
                }
                // 将connfd添加epoll监视并设为非阻塞
                char clientIP[64];
                inet_ntop(AF_INET, &client_address.sin_addr.s_addr, clientIP, 64);
                printf("welcome connection:%s\n", clientIP);
                echos[connfd]->init(connfd, client_address);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务器端关闭连接
            }
            else if (events[i].events & EPOLLIN)
            {
                // 处理客户连接上接收到的数据
                // 主线程接收数据
                if (echos[sockfd]->read_once())
                {
                    // 交由线程池发送数据
                    // 在process中重置EPOLLONESHOT
                    m_pool->append(echos[sockfd], 1);
                }
            }
        }
    }
}

void echoServer::dealWithRead(int sockfd)
{
}

void echoServer::dealWithWrite(int sockfd)
{
}
