#include <iostream>
#include <unistd.h>
#include "threadpool/threadpool.h"

class test
{
public:
    static int count;
    void process()
    {
        int curcount = ++count;
        printf("%d\n", curcount);
    }
};

int test::count = 0;

int main()
{
    threadpool<test> pool;
    for (int i = 0; i < 100; ++i)
    {
        test t;
        pool.append(&t, 0);
    }
    sleep(2);
}