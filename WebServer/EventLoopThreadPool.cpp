
#include "EventLoopThreadPool.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, int numThreads)
:   baseLoop_(baseLoop),//绑定主线程
    started_(false),//是否已经启动了线程池的标记
    numThreads_(numThreads),//线程的个数
    next_(0)
{
    if (numThreads_ <= 0)
    {
        LOG << "numThreads_ <= 0";
        abort();
    }
}

/**
 * 启动和创建线程
 */
void EventLoopThreadPool::start()
{
    baseLoop_->assertInLoopThread();
    started_ = true;//是否已经启动了线程池的标记
    for (int i = 0; i < numThreads_; ++i)//创建若干个EventLoopThread线程
    {
        std::shared_ptr<EventLoopThread> t(new EventLoopThread());//创建EventLoopThread线程
        threads_.push_back(t);
        loops_.push_back(t->startLoop());//启动EventLoopThread线程，在进入事件循环之前，会调用cb
    }
}

/**
 * 当新当一个连接到来时，我们需要选择一个EventLoop对象来处理
 */
EventLoop *EventLoopThreadPool::getNextLoop()
{
    baseLoop_->assertInLoopThread();
    assert(started_);
    EventLoop *loop = baseLoop_;//baseLoop_就是mainReator
    //如果loops为空，则loop指向baseloop
    //如果不为空，按照round-robin（RR 轮询）当调度方式选择一个EventLoop
    if (!loops_.empty())//如果loops_为空则没有subReator只有一个mainReator，返回mainReator进行处理
    {
        //round-robin
        loop = loops_[next_];
        next_ = (next_ + 1) % numThreads_;
    }
    return loop;
}