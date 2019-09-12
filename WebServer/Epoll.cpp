
#include "Epoll.h"
#include "Util.h"
#include "base/Logging.h"
#include <sys/epoll.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <queue>
#include <deque>
#include <assert.h>

#include <arpa/inet.h>
#include <iostream>
using namespace std;


const int EVENTSNUM = 4096;
const int EPOLLWAIT_TIME = 10000;

typedef shared_ptr<Channel> SP_Channel;

Epoll::Epoll():
    epollFd_(epoll_create1(EPOLL_CLOEXEC)),
    events_(EVENTSNUM)
{
    assert(epollFd_ > 0);
}
Epoll::~Epoll()
{ }

//注册channel
// 注册新描述符
void Epoll::epoll_add(SP_Channel request, int timeout)
{
    int fd = request->getFd();
    if (timeout > 0)
    {
        add_timer(request, timeout);
        fd2http_[fd] = request->getHolder();
    }
    struct epoll_event event;
    event.data.fd = fd;
    event.events = request->getEvents();

    request->EqualAndUpdateLastEvents();//更新lastevent

    fd2chan_[fd] = request;
    if(epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) < 0)
    {
        perror("epoll_add error");
        fd2chan_[fd].reset();
    }
}

//更新channel
// 修改描述符状态
void Epoll::epoll_mod(SP_Channel request, int timeout)
{
    if (timeout > 0)
        add_timer(request, timeout);
    int fd = request->getFd();
    if (!request->EqualAndUpdateLastEvents())//如果修改前和修改后的event是没有变化的，就不用处理了
    {
        struct epoll_event event;
        event.data.fd = fd;
        event.events = request->getEvents();
        if(epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event) < 0)
        {
            perror("epoll_mod error");
            fd2chan_[fd].reset();
        }
    }
}

//删除channel
// 从epoll中删除描述符
void Epoll::epoll_del(SP_Channel request)
{
    int fd = request->getFd();
    struct epoll_event event;
    event.data.fd = fd;
    event.events = request->getLastEvents(); 
    //event.events = 0;
    // request->EqualAndUpdateLastEvents()
    if(epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &event) < 0)
    {
        perror("epoll_del error");
    }
    fd2chan_[fd].reset();
    fd2http_[fd].reset();
}




// 返回活跃事件数
std::vector<SP_Channel> Epoll::poll()
{
    while (true)
    {
        //event_count：活跃的事件个数   
        int event_count = epoll_wait(epollFd_, &*events_.begin(), events_.size(), EPOLLWAIT_TIME);
        if (event_count < 0)
            perror("epoll wait error");
        std::vector<SP_Channel> req_data = getEventsRequest(event_count);//把事件放到通道里面
        if (req_data.size() > 0)
            return req_data;
    }
}

void Epoll::handleExpired()
{
    timerManager_.handleExpiredEvent();
}

// 分发处理函数
//处理epoll返回的事件
std::vector<SP_Channel> Epoll::getEventsRequest(int events_num)
{
    std::vector<SP_Channel> req_data;
    for(int i = 0; i < events_num; ++i)
    {
        // 获取有事件产生的描述符
        int fd = events_[i].data.fd;
        //查找当前描述符属于哪个channel
        SP_Channel cur_req = fd2chan_[fd];
            
        if (cur_req)//如果通道存在
        {
            cur_req->setRevents(events_[i].events);//给channel设置epoll返回的事件
            cur_req->setEvents(0);//清空我们channel关注的事件
            // 加入线程池之前将Timer和request分离
            //cur_req->seperateTimer();
            req_data.push_back(cur_req);//返回活跃的连接集合 
        }
        else
        {
            LOG << "SP cur_req is invalid";
        }
    }
    return req_data;//返回到Eventloop当中，Eventloop遍历这些通道进行处理
}

void Epoll::add_timer(SP_Channel request_data, int timeout)
{
    shared_ptr<HttpData> t = request_data->getHolder();
    if (t)
        timerManager_.addTimer(t, timeout);
    else
        LOG << "timer add fail";
}