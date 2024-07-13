#include <iostream>
#include <unistd.h>
#include "threadpool.h"
#include "echoServer.h"

int main()
{
    echoServer server;
    server.init(9000, 8);
    server.thread_pool();
    server.eventListen();
    server.eventLoop();
}