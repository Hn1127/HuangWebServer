#include "log.h"

const char *log_level[5] = {"[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: "};

void Log::init(const char *base_filename, int close_log, int log_buf_size, int split_lines)
{
}

void Log::write_log(int level, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    vsnprintf(buf, 1024, format, args);
    printf("%s %s\n", log_level[level], buf);
    va_end(args);
}

void Log::async_write_log()
{
}
