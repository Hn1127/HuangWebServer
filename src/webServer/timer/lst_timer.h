#pragma once

// 定时器
class timer
{
};

// 定时器链表
class lst_timer
{
public:
    // 进行一次心跳检测，设置ALARM信号
    void tick();
};