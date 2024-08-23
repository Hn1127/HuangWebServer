#include <iostream>
#include <unistd.h>
#include "threadpool.h"
#include "echoServer.h"
#include "webServer.h"

#define ISECHOSERVER false
#define ISWEBSERVER true

int main()
{
    if (ISECHOSERVER)
    {
        // echo服务器
        echoServer server;
        server.init(9000, 8);
        server.run();
    }
    else if (ISWEBSERVER)
    {
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