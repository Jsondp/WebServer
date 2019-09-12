
#pragma once
#include "base/noncopyable.h"
#include "EventLoopThread.h"
#include "base/Logging.h"
#include <memory>
#include <vector>
/**
 * I/O线程池的功能是开启若干个I/O线程，并让这些I/O线程处于事件循环的状态
 */
class EventLoopThreadPool : noncopyable
{
public:
    EventLoopThreadPool(EventLoop* baseLoop, int numThreads);

    ~EventLoopThreadPool()
    {
        LOG << "~EventLoopThreadPool()";
    }
    void start();

    EventLoop *getNextLoop();

private:
    EventLoop* baseLoop_;//与Acceptor所属Eventloop相同
    bool started_;//是否开启的标志
    int numThreads_;//线程数
    int next_;//新连接到来，所选择的EventLoop对象下标 
    //I/O线程列表，当vectot销毁，它所管理的EventLoopThread对象也就销毁了
    std::vector<std::shared_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;//EventLoop列表，不需要EventLoopThread对象来销毁，自行销毁
};