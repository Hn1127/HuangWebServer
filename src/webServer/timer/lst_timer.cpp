#include "lst_timer.h"

lst_timer::lst_timer()
{
    head = std::shared_ptr<timer>(new timer);
    tail = std::shared_ptr<timer>(new timer);
    head->next = tail;
    tail->prev = head;
    m_epollfd = 0;
    m_TIMESHOT = 5;
}

lst_timer::lst_timer(int epollfd)
{
    head = std::shared_ptr<timer>(new timer);
    tail = std::shared_ptr<timer>(new timer);
    head->next = tail;
    tail->prev = head;
    m_epollfd = epollfd;
    m_TIMESHOT = 5;
}

void lst_timer::tick()
{
    LOG_INFO("webserver tick");
    // 遍历升序链表，移除定时器并关闭fd
    std::shared_ptr<timer> cur_timer = head->next;
    time_t cur_time = time(NULL);
    while (cur_timer != tail)
    {
        if (cur_timer->expire_time > cur_time)
        {
            break;
        }
        cur_timer = cur_timer->next;
        deal_timer(cur_timer->prev);
    }
    alarm(m_TIMESHOT);
}

void lst_timer::add_timer(std::shared_ptr<timer> t_timer)
{
    // 从后向前升序链表，找到第一个小于目标的
    std::shared_ptr<timer> cur_timer = tail->prev;
    if (head->next == tail)
    {
        // 无内容
        t_timer->prev = head;
        t_timer->next = tail;
        head->next = t_timer;
        tail->prev = t_timer;
        return;
    }
    while (cur_timer != head)
    {
        // 找到第一个不大于目标的
        if (cur_timer->expire_time <= t_timer->expire_time)
        {
            // 添加timer进来
            t_timer->prev = cur_timer;
            t_timer->next = cur_timer->next;
            cur_timer->next = t_timer;
            t_timer->next->prev = t_timer;
            break;
        }
        cur_timer = cur_timer->prev;
    }
}

void lst_timer::adjust_timer(std::shared_ptr<timer> t_timer)
{
    // 将t_timer从原有位置删除,再添加进来
    t_timer->prev->next = t_timer->next;
    t_timer->next->prev = t_timer->prev;
    add_timer(t_timer);
}

void lst_timer::deal_timer(std::shared_ptr<timer> t_timer)
{
    // 移出事件表关闭fd
    epoll_ctl(m_epollfd, EPOLL_CTL_DEL, t_timer->sockfd, 0);
    close(t_timer->sockfd);
    // 删除定时器
    // 上一定时器的next指向该定时器next
    t_timer->prev->next = t_timer->next;
    t_timer->next->prev = t_timer->prev;
}

void timer::delay_time(int TIMESHOT)
{
    expire_time += TIMESHOT * 3;
}
