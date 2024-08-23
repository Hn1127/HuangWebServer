#pragma once

#include <stdarg.h>
#include <mutex>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
// 异步日志

class Log
{
public:
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    // 日志线程的入口函数
    static void *log_thread_func(void *args)
    {
        Log::get_instance()->async_write_log();
        return nullptr;
    }

    void init(const char *base_filename, int close_log = false, int log_buf_size = 8192, int split_lines = 5000000);

    // 外部调用该函数进行异步写日志
    void write_log(int level, const char *format, ...);

private:
    void async_write_log();
    char buf[1024];
};

#define LOG_ERROR(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__);
#define LOG_WARN(format, ...) Log::get_instance()->write_log(1, format, ##__VA_ARGS__);
#define LOG_INFO(format, ...) Log::get_instance()->write_log(2, format, ##__VA_ARGS__);
#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__);