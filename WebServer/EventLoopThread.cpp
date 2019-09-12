
#include "EventLoopThread.h"
#include <functional>


EventLoopThread::EventLoopThread()
:   loop_(NULL),
    exiting_(false),
    thread_(bind(&EventLoopThread::threadFunc, this), "EventLoopThread"),//构造一个thread对象
    mutex_(),
    cond_(mutex_)//初始化条件变量
{ }

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != NULL)
    {
        loop_->quit();//退出I/O线程，让I/O线程的loop循环退出，从而退出来I/O线程
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    assert(!thread_.started());
    //创建线程，创建线程后会调用threadFunc
    //那么现在就有两个线程运行startLoop的线程和运行threadFunc的线程，这两个线程并发执行
    thread_.start();
    {
        MutexLockGuard lock(mutex_);
        // 一直等到threadFun在Thread里真正跑起来
        while (loop_ == NULL)//loop_不为空才放行，代表threadFunc先执行
            cond_.wait();
    }
    return loop_;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;

    {
        MutexLockGuard lock(mutex_);
        //loop_指针指向一个栈上的对象，threadFunc函数退出之后，这个指针就失效了
        //threadFunc函数的退出，就意味着线程退出，EventLoopThread对象也没有存在的价值了。也应该被退出
        //线程失效了程序也结束了，所以不会出现什么大的问题
        loop_ = &loop;
        cond_.notify();
    }

    loop.loop();
    //assert(exiting_);
    loop_ = NULL;
}