#include "http.h"

bool http::isReactor = true;
bool http::isConnfdET = true;
int http::m_epollfd = 0;
char *http::m_server_root = nullptr;

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

http::http()
{
}

http::~http()
{
}

void http::process()
{
    LOG_DEBUG("---sockfd %d start process---", m_connfd);
    if (isReactor)
    {
        // Reactor模式
        // 接受读写就绪事件
        LOG_DEBUG("Reactor");
        if (m_state == 0)
        {
            // 读状态
            // 进行读之后再进行处理，将处理结果放入写缓冲区
            if (read_once())
            {
                // 接收无误后进行处理
                HTTP_CODE ret = process_request();
                // ret返回值 NO_REQUEST, NO_RESOURCE, FORBIDDEN, BAD_REQUEST, FILE_REQUEST, INTERNAL_ERROR
                if (ret == NO_REQUEST)
                {
                    // 请求不完整,再次添加EPOLLIN事件
                    LOG_DEBUG("NOREQUEST");
                    modfd(m_epollfd, m_connfd, EPOLLIN, true, isConnfdET);
                    return;
                }
                if (process_response(ret))
                {
                    // 写缓冲区正常
                    // 更新定时器
                    // 添加EPOLLOUT
                    modfd(m_epollfd, m_connfd, EPOLLOUT, true, isConnfdET);
                }
                else
                {
                    // 写缓冲区有误
                    // 关闭连接
                }
            }
            else
            {
                // 缓冲区已满
                // 非正常退出
                // 对端关闭
                // 不重置EPOLLIN,等待tick自动关闭连接
                return;
            }
        }
        else
        {
            // 写状态
            if (write_once())
            {
                // 写无误
                // 更新定时器
                return;
            }
            else
            {
                // 写出错
                // 不重置EPOLLIN,等待tick自动关闭连接
                return;
            }
        }
    }
    else
    {
        // Proactor模式
        // 直接对读缓冲区中内容进行处理，处理结果放入写缓冲区
        // Proactor不会处理写事件,写事件将由主线程进行
        LOG_DEBUG("Proactor");
        HTTP_CODE ret = process_request();
        // ret返回值 NO_REQUEST, NO_RESOURCE, FORBIDDEN, BAD_REQUEST, FILE_REQUEST, INTERNAL_ERROR
        if (ret = NO_REQUEST)
        {
            // 请求不完整,再次添加EPOLLIN事件
            modfd(m_epollfd, m_connfd, EPOLLIN, true, isConnfdET);
            return;
        }
        if (process_response(ret))
        {
            // 写缓冲区正常
            // 更新定时器
            // 添加EPOLLOUT
            modfd(m_epollfd, m_connfd, EPOLLOUT, true, isConnfdET);
        }
        else
        {
            // 写缓冲区有误
            // 关闭连接
        }
    }
}

int http::setnonblock(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void http::addfd(int epollfd, int fd, bool ONESHOT, bool isETTRIG)
{
    epoll_event event;
    event.data.fd = fd;

    // 添加读事件和对端终止事件，开启ET模式
    if (isETTRIG)
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    // 添加ONESHOT
    if (ONESHOT)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblock(fd);
}

void http::modfd(int epollfd, int fd, int ev, bool ONESHOT, bool isETTRIG)
{
    epoll_event event;
    event.data.fd = fd;

    // 添加对端终止事件，ET模式
    if (isETTRIG)
        event.events = ev | EPOLLET | EPOLLRDHUP;
    else
        event.events = ev | EPOLLRDHUP;

    // 重置ONESHOT
    if (ONESHOT)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void http::removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void http::init(int connfd, std::shared_ptr<sockaddr_in> addr)
{
    LOG_DEBUG("main: init connfd %d", connfd);
    m_connfd = connfd;
    m_addr = addr;
    init();
}

void http::init()
{
    m_state = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    m_parse_idx = 0;
    m_line_start = 0;
    m_parse_status = PARSE_REQUESTLINE;
    m_method = GET;
    m_content_length = 0;
    m_linger = false;
    m_iv[0].iov_base = nullptr;
    m_iv[1].iov_base = nullptr;
}

bool http::read_once()
{
    LOG_DEBUG("%d read_once", m_connfd);
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        // 读缓冲区已满
        return false;
    }
    int bytes_read = 0; // 每次recv的字节数
    if (isConnfdET)
    {
        // ET触发模式
        // 需要循环进行读取(未读取完本次不会再有EPOLLIN信号)
        while (true)
        {
            bytes_read = recv(m_connfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            LOG_DEBUG("bytes_read %d", bytes_read);
            LOG_DEBUG("read_buf\n----------\n%s\n----------", (m_read_buf));
            if (bytes_read == -1)
            {
                if (errno == EAGAIN)
                {
                    // 正常退出
                    LOG_DEBUG("read_buf_len %d", strlen(m_read_buf));
                    return true;
                }
                return false;
            }
            else if (bytes_read == 0)
            {
                // 对端已经关闭
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
    else
    {
        // LT触发方式
        // 进行一次读取即可(若未读取完仍会有EPOLLIN信号)
        // 此时有EPOLLIN信号，recv应该定能接受到
        bytes_read = recv(m_connfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read <= 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
        return true;
    }
}
bool http::write_once()
{
    // 将写缓冲区中内容集中写到对端
    if (m_bytes_to_send == 0)
    {
        // 写缓冲区无内容
        modfd(m_epollfd, m_connfd, EPOLLIN, isConnfdET);
        init();
    }
    else
    {
        // 写缓冲区中有内容
        int bytes_send = 0; // 已发送的字节数
        // 循环进行发送
        while (true)
        {
            bytes_send = writev(m_connfd, m_iv, m_iv_count);
            LOG_DEBUG("bytes_send %d", bytes_send);
            if (bytes_send == -1)
            {
                if (errno == EAGAIN)
                {
                    // 正常退出
                    // 重置EPOLLOUT事件
                    modfd(m_epollfd, m_connfd, EPOLLOUT, true, isConnfdET);
                    return true;
                }
                // 若非正常退出则要取消映射
                unmap();
                return false;
            }
            m_bytes_have_send += bytes_send;
            m_bytes_to_send -= bytes_send;
            if (m_bytes_to_send == 0)
            {
                // 写完毕
                unmap();
                modfd(m_epollfd, m_connfd, EPOLLIN, true, isConnfdET);
                // 是否重置状态
                if (m_linger == true)
                {
                    // 持久连接
                    init();
                    return true;
                }
                else
                {
                    return true;
                }
            }
            // 更新集中写参数
            if (m_bytes_have_send < m_write_idx)
            {
                // 第一块还未写完
                // 更新第一块位置
                m_iv[0].iov_base = m_write_buf + m_bytes_have_send;
                m_iv[0].iov_len = m_iv[0].iov_len - m_bytes_have_send;
            }
            else
            {
                // 第一块已经写完
                m_iv[0].iov_len = 0;
                m_iv[1].iov_base = m_file_address + (m_bytes_have_send - m_write_idx);
                m_iv[1].iov_len = m_bytes_to_send;
            }
        }
    }

    return true;
}

http::LINE_STATE http::checkLine()
{
    // 解析当前行，直到下一个\r\n出现或者到达读缓冲区末尾
    // 由于有ONESHOT，此时只会有一个线程处理该任务
    long end_idx = m_read_idx - 1;
    for (; m_parse_idx < end_idx; ++m_parse_idx)
    {
        char temp = m_read_buf[m_parse_idx];
        if (temp == '\r')
        {
            // 考察下一个是不是\n
            if (m_read_buf[m_parse_idx + 1] == '\n')
            {
                m_read_buf[m_parse_idx++] = '\0';
                m_read_buf[m_parse_idx++] = '\0';
                // 此时parse_idx移动到下一行开始位置
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            // 考察上一个是不是\r
            if (m_parse_idx > 1 && m_read_buf[m_parse_idx - 1] == '\r')
            {
                m_read_buf[m_parse_idx - 1] = '\0';
                m_read_buf[m_parse_idx++] = '\0';
                // 此时parse_idx移动到下一行开始位置
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

http::LINE_STATE http::getLine(char *&curLine)
{
    http::LINE_STATE ret = checkLine();
    if (ret == LINE_OK)
    {
        curLine = m_read_buf + m_line_start;
        m_line_start = m_parse_idx;
    }
    return ret;
}

http::HTTP_CODE http::process_request()
{
    // 解析读缓冲区中的内容
    LINE_STATE line_state = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *curLine = nullptr;

    LOG_DEBUG("sockfd %d process_request", m_connfd);

    while (true)
    {
        // 一直解析，直到到达读缓冲区末尾或解析完毕
        // 分割下一行

        if (m_parse_status == PARSE_CONTENT)
        {
            // 若存在消息体，则当前parse_idx指向消息体内容
            // 解析消息体时不分行
            // 检查是否达到m_content_length长度
            // 未达到则需要再次读
            if (line_state == LINE_OPEN)
                break;
        }
        else
        {
            line_state = getLine(curLine);
            if (line_state != LINE_OK)
            {
                // 可能为LINE_BAD / LINE_OPEN
                // 出现错误的格式
                // 行不完整，说明需要再一次read
                break;
            }
        }

        // 根据parse_status和curLine
        if (m_parse_status == PARSE_REQUESTLINE)
        {
            ret = parse_request_line(curLine);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
        }
        else if (m_parse_status == PARSE_HEADER)
        {
            ret = parse_request_header(curLine);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            if (ret == GET_REQUEST)
            {
                // 无消息体,为GET方法
                return parse_request();
            }
        }
        else if (m_parse_status == PARSE_CONTENT)
        {
            ret = parse_request_content(curLine + 2);
            if (ret == GET_REQUEST)
            {
                // 全部读完毕
                return parse_request();
            }
            // 未读全
            line_state = LINE_OPEN;
        }
        else
        {
            return INTERNAL_ERROR;
        }
    }

    return NO_REQUEST;
}

http::HTTP_CODE http::parse_request_line(char *curLine)
{
    // 格式 GET /index/target HTTP/1.1 \r\n
    // 已在checkLine中将 \r\n置为\0\0
    // METHOD URL VERSION \0\0
    m_url = strchr(curLine, ' ');
    if (!m_url)
    {
        // 没有出现\t
        return BAD_REQUEST;
    }
    // 此时m_url指向' '，将其置为0并向前移动一位指向URL起始位置
    *m_url++ = '\0';
    // METHOD \0 URL VERSION \0\0
    // 判断METHOD
    char *method = curLine;
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
    }
    else
    {
        return BAD_REQUEST;
    }
    m_version = strchr(m_url, ' ');
    if (!m_version)
    {
        // 没有出现\t
        return BAD_REQUEST;
    }
    // 此时m_version指向' '，将其置为0并向前移动一位指向version起始位置
    *m_version++ = '\0';
    // METHOD \0 URL \0 VERSION \0\0
    // 只接受HTTP1.1
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }
    // 跳过https前缀
    // 使得url指向最后一个/
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
    }
    // 不接受https请求
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        return BAD_REQUEST;
    }
    m_parse_status = PARSE_HEADER;
    return NO_REQUEST;
}

http::HTTP_CODE http::parse_request_header(char *curLine)
{
    // 解析请求头
    // 请求头和请求体由\r\n隔开

    if (curLine[0] == '\0')
    {
        // curLine由getLine()得到，将\r\n置为\0
        // curLine[0]若为\0则说明请求头结束
        // 检验是否有Content-Length字段
        if (m_content_length != 0)
        {
            m_parse_status = PARSE_CONTENT;
            return NO_REQUEST;
        }
        // 没有Content-Length字段则为GET方法
        return GET_REQUEST;
    }
    else if (strncasecmp(curLine, "Host: ", 6) == 0)
    {
        // 匹配Host字段
        curLine += 6; // 指向ip:port
        m_host = curLine;
    }
    else if (strncasecmp(curLine, "Connection: ", 12) == 0)
    {
        // 匹配Cnnection字段
        curLine += 12; // 指向keep-alive/close
        if (strcasecmp(curLine, "keep-alive") == 0)
        {
            // 持久连接
            m_linger = true;
        }
    }
    else if (strncasecmp(curLine, "Content-Length: ", 16) == 0)
    {
        // 匹配Content-Length:字段
        curLine += 16; // 指向 length
        m_content_length = atol(curLine);
    }
    else
    {
        // 其余请求头暂不处理
    }
    return NO_REQUEST;
}

http::HTTP_CODE http::parse_request_content(char *curLine)
{
    // 判断是否接受了完整的消息体
    // curLine指向请求体的起始位置
    // read_idx为接受的内容的长度,parse_idx为请求体的起始位置
    // parse_idx+content_length为期望的结束位置
    // read_idx为实际的结束位置
    if (m_read_idx < (m_parse_idx + m_content_length))
    {
        // 不完整,需要再次进行读
        return NO_REQUEST;
    }
    // 读写完整
    // 为末尾添加\0
    curLine[m_content_length] = '\0';
    m_content = curLine;
    LOG_DEBUG("parse_request_content:\n%s", curLine);

    return GET_REQUEST;
}

http::HTTP_CODE http::parse_request()
{
    // 请求分割接收完毕,根据URL进行进一步判断
    // 此时m_server_root是 /path/root
    strcpy(m_filename, m_server_root);
    if (m_method == POST)
    {
        // 解析POST请求附带的参数
        // 实际上涉及到的内容为username和password
        // 使用form提交的表单为 key=value&key=value
    }
    // 路径映射
    if (strcmp(m_url, "/") == 0)
    {
        strcat(m_filename, "/index.html");
    }
    else
    {
        strcat(m_filename, m_url);
    }
    // 判断filename是否存在
    if (stat(m_filename, &m_file_state) < 0)
        return NO_RESOURCE;
    // 判断是否是目录文件
    if (S_ISDIR(m_file_state.st_mode))
        return BAD_REQUEST;
    // 判断是否有读权限
    if (!(m_file_state.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    LOG_DEBUG("request url: %s", m_url);
    LOG_DEBUG("request file: %s", m_filename);
    int filefd = open(m_filename, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_state.st_size, PROT_READ, MAP_PRIVATE, filefd, 0);
    close(filefd);
    return FILE_REQUEST;
}

bool http::process_response(HTTP_CODE request_code)
{
    // 根据请求解析结果，添加响应头和响应正文
    // 响应头为自己添加，正文为文件映射
    // INTERNAL_ERROR, NO_RESOURCE, FORBIDDEN, BAD_REQUEST, FILE_REQUEST
    switch (request_code)
    {
    case INTERNAL_ERROR:
    {
        // 服务器处理时出现问题
        add_response_line(500, error_500_title);
        add_response_header(strlen(error_500_form));
        // 理论上,写入缓冲区不会false
        // return false后将会直接关闭连接
        if (!add_response_content(error_500_form))
            return false;
        break;
    }
    case NO_RESOURCE:
    {
        // 没有对应的文件
        add_response_line(404, error_404_title);
        add_response_header(strlen(error_404_form));
        if (!add_response_content(error_404_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        // 文件所指是个目录
        add_response_line(404, error_404_title);
        add_response_header(strlen(error_404_form));
        if (!add_response_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_response_line(403, error_403_title);
        add_response_header(strlen(error_403_form));
        if (!add_response_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_response_line(200, ok_200_title);
        if (m_file_state.st_size == 0)
        {
            // 空文件
            const char *ok_string = "<html><body></body></html>";
            add_response_header(strlen(ok_string));
            if (!add_response_content(ok_string))
                return false;
            break;
        }
        break;
    }
    default:
    {
        return false;
    }
    }
    // 若写缓冲区时未出错,则会到达该处
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    LOG_DEBUG("m_write_idx1 %d", m_write_idx);
    m_iv_count = 1;
    m_bytes_to_send = m_write_idx;
    if (request_code == FILE_REQUEST && m_file_state.st_size != 0)
    {
        // 能成功地映射文件
        add_response_header(m_file_state.st_size);
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_state.st_size;
        m_iv_count = 2;
        m_bytes_to_send = m_write_idx + m_file_state.st_size;
    }
    return true;
}

bool http::add_response(const char *format, ...)
{
    // 成功加入则return true否则false
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list args;
    va_start(args, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, args);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(args);
        return false;
    }
    m_write_idx += len;
    va_end(args);

    return true;
}

bool http::add_response_line(int status, const char *title)
{
    return add_response("HTTP/1.1 %d %s\r\n", status, title);
}

bool http::add_response_header(int content_len)
{
    // 响应正文长度
    return add_response("Content-Length:%d\r\n", content_len) &&
           add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close") &&
           add_response("\r\n");
}

bool http::add_response_content(const char *content)
{
    return add_response("%s", content);
}

void http::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_state.st_size);
        m_file_address = 0;
    }
}
