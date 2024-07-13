#pragma once

// 被threadpool处理的任务
// 所有任务必须继承该类
class handler
{
public:
    virtual void process() = 0;
};