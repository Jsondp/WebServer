
#pragma once
#include "base/Condition.h"
#include "base/MutexLock.h"
#include "base/Thread.h"
#include "base/noncopyable.h"
#include "EventLoop.h"

class EventLoopThread :noncopyable
{
public:
    EventLoopThread();
    ~EventLoopThread();
    //启动线程，会调用Thread中的start函数
    //该线程会成为I/O线程
    EventLoop* startLoop();

private:
    void threadFunc();//线程函数
    EventLoop *loop_;//loop_指向一个EventLoop对象
    bool exiting_;//是否退出
    Thread thread_;//基于对象的编程思想，包含来一个Thread对象
    MutexLock mutex_;
    Condition cond_;
};