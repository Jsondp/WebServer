
#pragma once
#include "Channel.h"
#include "HttpData.h"
#include "Timer.h"
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>
#include <memory>

class Epoll
{
public:
    Epoll();
    ~Epoll();
    void epoll_add(SP_Channel request, int timeout);
    void epoll_mod(SP_Channel request, int timeout);
    void epoll_del(SP_Channel request);

    std::vector<std::shared_ptr<Channel>> poll();//返回活动的通道列表
    std::vector<std::shared_ptr<Channel>> getEventsRequest(int events_num);
    void add_timer(std::shared_ptr<Channel> request_data, int timeout);
    int getEpollFd()
    {
        return epollFd_;
    }
    void handleExpired();
private:
    static const int MAXFDS = 100000;
    int epollFd_;//epoll的文件描述符
    std::vector<epoll_event> events_;//我们关注事件数组
    std::shared_ptr<Channel> fd2chan_[MAXFDS];//我们关注的通道列表，key是文件描述符，value是channel*
    std::shared_ptr<HttpData> fd2http_[MAXFDS];//我们关注的通道列表，key是文件描述符，value是HttpData*
    TimerManager timerManager_;//定时器
};