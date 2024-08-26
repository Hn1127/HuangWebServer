#include <iostream>
#include <unistd.h>
#include "echoServer.h"
#include "webServer.h"

#define ISECHOSERVER false
#define ISWEBSERVER true

void LogInit(std::string log_dir_name, std::string basename, bool open_log, int max_lines)
{
    Log::get_instance()->init(log_dir_name.c_str(), basename.c_str(), open_log, max_lines);
    LOG_EMPTY("--------------------------------------------------------------------------\n");
    LOG_INFO("Log Init done");
}

void InitServer(std::string log_dir_name, std::string basename, bool open_log = true, int max_lines = 500000)
{
    LogInit(log_dir_name, basename, open_log, max_lines);
}

int main()
{
    if (ISECHOSERVER)
    {
        InitServer("EchoLogFile", " ServerLog");
        // echo服务器
        echoServer server;
        server.init(9000, 8);
        server.run();
    }
    else if (ISWEBSERVER)
    {
        InitServer("WebLogFile", "ServerLog", true, 100);
        // web服务器
        std::string url = "localhost";
        std::string user = "root";
        std::string password = "sanduo";
        std::string dbName = "HuangWebServer";
        int sql_num = 8;
        webServer server;
        server.init(url, user, password, dbName, sql_num, 9000, 8);
        server.run();
    }
}