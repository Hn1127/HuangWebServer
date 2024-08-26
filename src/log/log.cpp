#include "log.h"

const char *log_level[5] = {"[ERROR] ", "[WARN]  ", "[INFO]  ", "[DEBUG] "};

void Log::init(const char *dir_name, const char *base_filename, bool open_log, int max_lines)
{
    m_count = 0;
    m_today = -1;
    m_max_lines = max_lines;
    m_filebase_name = base_filename;
    m_close_log = !open_log;

    // 初始化两个缓冲区队列
    m_ptr_bufvec1 = ptr_bufvec(new ptr_vec(1, ptr_buf(new buf_block)));
    m_ptr_bufvec2 = ptr_bufvec(new ptr_vec(1, ptr_buf(new buf_block)));
    m_cur_buf_idx = 0;

    // 创建日志文件
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    m_today = sys_tm->tm_yday;
    char file_fullname[128];
    snprintf(file_fullname, 128, "%s/%s_%d_%02d_%02d.log", dir_name, base_filename,
             sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday);
    m_fp = fopen(file_fullname, "a");
    if (m_fp == NULL)
    {
        m_close_log = true;
        printf("Log open file failed, close log:%s\n", file_fullname);
    }

    // 启动线程
    thread_close = false;
    log_thread = new std::thread(log_thread_func);
    log_thread->detach();
}

void Log::close_log()
{
    m_close_log = true;
    while (!thread_close)
        ;
    // 线程结束后,回收资源
    fclose(m_fp);
}

void Log::write_log(int level, const char *format, ...)
{
    if (m_close_log)
        return;
    va_list args;
    va_start(args, format);
    char logline[1024];
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    // 格式化日志
    // yyyy-mm-dd-hh-mm-ss:[level] message \n
    snprintf(logline, 1024, "%d-%02d-%02d %02d:%02d:%02d %s ",
             sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday,
             sys_tm->tm_hour, sys_tm->tm_min, sys_tm->tm_sec, log_level[level]);
    vsnprintf(logline + strlen(logline), 1024, format, args);
    strcat(logline, "\n");
    // 向bufvec1中写入
    write_log_to_buf(logline);
    va_end(args);
}

void Log::write_log_empty(const char *line)
{
    write_log_to_buf(line);
}

void Log::write_log_to_buf(const char *logline)
{
    int bytes_to_send = strlen(logline);
    {
        std::lock_guard<std::mutex> lkg(mtx);

        // 判断当前写入缓冲块是否能容纳当前行+等级
        if (BUF_BLOCK_SIZE - m_ptr_bufvec1->at(m_cur_buf_idx)->idx < bytes_to_send)
        {
            // 已满,需要申请新的缓冲块
            m_ptr_bufvec1->emplace_back(ptr_buf(new buf_block));
            ++m_cur_buf_idx;
        }
        // 将其写入
        ptr_buf cur_block = m_ptr_bufvec1->at(m_cur_buf_idx);
        strcpy(cur_block->buf + cur_block->idx, logline);
        cur_block->idx += bytes_to_send;
        ++m_line_count;
    }
}

void Log::async_write_log()
{
    // 线程工作在这个函数上，负责周期的将缓冲块的内容刷入文件
    // 使用条件变量
    thread_close = false;
    while (!m_close_log)
    {
        // 循环等待信号(或者5秒后)将缓冲块内容刷入文件
        auto pred_time = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        int cur_lines = 0;
        {
            std::unique_lock<std::mutex> lck(mtx);
            cv.wait_until(lck, pred_time);
            // 交换指针
            cur_lines = m_line_count;
            m_line_count = 0;
            m_cur_buf_idx = 0;
            m_ptr_bufvec1.swap(m_ptr_bufvec2);
        }
        // 处理ptr_bufvec2的内容
        time_t t = time(NULL);
        struct tm *sys_tm = localtime(&t);
        // 判断是否到了下一天
        if (sys_tm->tm_yday != m_today)
        {
            // 到了下一天,需要创建新日志文件
            fclose(m_fp);
            m_count = 0;
            m_today = sys_tm->tm_yday;
            char file_fullname[128];
            snprintf(file_fullname, 128, "%s/%s_%d_%02d_%02d.log", m_dir_name.c_str(), m_filebase_name.c_str(),
                     sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday);
            m_fp = fopen(file_fullname, "a");
            if (m_fp == NULL)
            {
                m_close_log = true;
                printf("Log open file failed, close log:%s\n", file_fullname);
            }
        }
        // 判断是否超过最大行数
        if (cur_lines >= m_max_lines)
        {
            // 超过行数需要创建新的文件写入
            fclose(m_fp);
            ++m_count;
            m_today = sys_tm->tm_yday;
            char file_fullname[128];
            snprintf(file_fullname, 128, "%s/%s_%d_%02d_%02d_%d.log", m_dir_name.c_str(), m_filebase_name.c_str(),
                     sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday, m_count);
            m_fp = fopen(file_fullname, "a");
            if (m_fp == NULL)
            {
                m_close_log = true;
                printf("Log open file failed, close log:%s\n", file_fullname);
            }
        }
        for (auto &bufptr : *m_ptr_bufvec2)
        {
            // 将buf中idx内容刷入文件
            bufptr->buf[bufptr->idx] = '\0';
            fputs(bufptr->buf, m_fp);
            bufptr->idx = 0;
        }
        fflush(m_fp);
        // 重新规划大小
        m_ptr_bufvec2->resize(1);
    }
    thread_close = true;
}
