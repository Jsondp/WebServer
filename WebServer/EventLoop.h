
#pragma once
#include "base/Thread.h"
#include "Epoll.h"
#include "base/Logging.h"
#include "Channel.h"
#include "base/CurrentThread.h"
#include "Util.h"
#include <vector>
#include <memory>
#include <functional>

#include <iostream>
using namespace std;

/*
    EventLoop：One loop per thread意味着每个线程只能有一个EventLoop对象，
    EventLoop即是时间循环，每次从poller里拿活跃事件，
    并给到Channel里分发处理。EventLoop中的loop函数会在最底层(Thread)中被真正调用，
    开始无限的循环，直到某一轮的检查到退出状态后从底层一层一层的退出。
 */
class EventLoop
{
public:
    typedef std::function<void()> Functor;
    EventLoop();
    ~EventLoop();
    void loop();
    void quit();
    void runInLoop(Functor&& cb);
    void queueInLoop(Functor&& cb);
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    void assertInLoopThread()
    {
        assert(isInLoopThread());
    }
    void shutdown(shared_ptr<Channel> channel)
    {
        shutDownWR(channel->getFd());
    }
    void removeFromPoller(shared_ptr<Channel> channel)//从poller中移除通道
    {
        //shutDownWR(channel->getFd());
        poller_->epoll_del(channel);
    }
    void updatePoller(shared_ptr<Channel> channel, int timeout = 0)//在poll中更新通道
    {
        poller_->epoll_mod(channel, timeout);
    }
    void addToPoller(shared_ptr<Channel> channel, int timeout = 0)//在poll中添加通道
    {
        poller_->epoll_add(channel, timeout);
    }
    
private:
/**
 * bool型是原子操作的
 */
    // 声明顺序 wakeupFd_ > pwakeupChannel_
    bool looping_;//当前是否处于循环的状态,原子性的操作
    //epoll的触发模式在这里我选择了ET模式
    //Epoll是对epoll的行为进行了封装
    shared_ptr<Epoll> poller_;
    int wakeupFd_;//用于eventfd
    bool quit_;//是否退出loop,原子性的操作
    bool eventHandling_;//当前是否处于事件处理的状态
    mutable MutexLock mutex_;//自己封装的线程锁
    std::vector<Functor> pendingFunctors_;//调用追加函数列表
    bool callingPendingFunctors_;//是否在调用追加的函数
    const pid_t threadId_;//当前对象所属线程ID
    shared_ptr<Channel> pwakeupChannel_;//该通道会纳入到poller来管理
    
    void wakeup();
    void handleRead();
    void doPendingFunctors();
    void handleConn();
};
