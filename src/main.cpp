#include <iostream>
#include <unistd.h>
#include "threadpool.h"
#include "echoServer.h"

#define ISECHOSERVER 1
#define ISWEBSERVER 0

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
    }
}