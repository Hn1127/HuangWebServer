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
        webServer server;
        server.init(9000, 8);
        server.run();
    }
}