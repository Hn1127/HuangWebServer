#pragma once

#include <stdarg.h>
#include <mutex>
#include <memory>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
// 异步日志

const int BUF_BLOCK_SIZE = 2048;

struct buf_block
{
    char buf[BUF_BLOCK_SIZE];
    long idx = 0;
};

class Log
{
public:
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    // 日志线程的入口函数
    static void log_thread_func()
    {
        Log::get_instance()->async_write_log();
        return;
    }

    // 初始化函数，用于指定文件前缀名，是否将日志输入到文件中
    void init(const char *dir_name, const char *base_filename, bool open_log = true, int max_lines = 500000);

    // 关闭日志
    void close_log();

    // 外部调用该函数进行异步写日志
    void write_log(int level, const char *format, ...);

    // 写入空白日志
    void write_log_empty(const char *line);

private:
    void async_write_log();
    void write_log_to_buf(const char *logline);

private:
    std::thread *log_thread;
    bool thread_close; // 线程是否结束
    // 日志文件变量
    std::string m_dir_name;      // 日志文件目录名
    std::string m_filebase_name; // 日志文件前缀名
    bool m_close_log;            // 是否开启日志
    int m_max_lines;             // 最大行数
    int m_line_count;            // 当前行数,一个日志文件不得超过最大行数
    int m_count;                 // 每天的日志文件编号
    int m_today;                 // 当前日志文件的天数
    FILE *m_fp;                  // 日志文件的FILE指针

    // 缓冲块buf_block->指向缓冲块的指针ptr_buf->指针队列ptr_vec->指向队列的指针m_ptr_bufvec
    typedef std::shared_ptr<buf_block> ptr_buf;
    typedef std::vector<ptr_buf> ptr_vec;
    typedef std::shared_ptr<ptr_vec> ptr_bufvec;
    ptr_bufvec m_ptr_bufvec1; // 用于接收日志的缓冲块队列
    ptr_bufvec m_ptr_bufvec2; // 在工作线程中与buf1进行交换
    int m_cur_buf_idx;        // 当前正在写入的缓冲块的队列下标

    // 互斥锁和条件变量，用于并发访问缓冲块队列
    std::mutex mtx;
    std::condition_variable cv;
};
#define LOG_EMPTY(logline) Log::get_instance()->write_log_empty(logline);
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__);
#define LOG_WARN(format, ...) Log::get_instance()->write_log(1, format, ##__VA_ARGS__);
#define LOG_INFO(format, ...) Log::get_instance()->write_log(2, format, ##__VA_ARGS__);
#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__);