
#include "EventLoop.h"
#include "base/Logging.h"
#include "Util.h"
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <iostream>
using namespace std;

//当前线程Eventloop对象指针
//线程局部存储
__thread EventLoop* t_loopInThisThread = 0;

int createEventfd()
{
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);//创建一个eventfd
    if (evtfd < 0)
    {
        LOG << "Failed in eventfd";
        abort();
    }
    return evtfd;
}

EventLoop::EventLoop()
:   looping_(false),
    poller_(new Epoll()),
    wakeupFd_(createEventfd()),//创建一个eventfd
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),
    pwakeupChannel_(new Channel(this, wakeupFd_))//创建一个eventfd的channel
{
    //如果当前线程已经创建了Eventloop对象，终止程序
    if (t_loopInThisThread)
    {
        //LOG << "Another EventLoop " << t_loopInThisThread << " exists in this thread " << threadId_;
    }
    else
    {
        t_loopInThisThread = this;
    }
    //设置唤醒channel的事件和相应的回调
    //pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
    pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));
    pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));
    poller_->epoll_add(pwakeupChannel_, 0);//让唤醒channel放到poll中来管理
}

void EventLoop::handleConn()
{
    //poller_->epoll_mod(wakeupFd_, pwakeupChannel_, (EPOLLIN | EPOLLET | EPOLLONESHOT), 0);
    updatePoller(pwakeupChannel_, 0);
}


EventLoop::~EventLoop()
{
    //wakeupChannel_->disableAll();
    //wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread = NULL;
}

//唤醒等待线程的函数
void EventLoop::wakeup()
{
    uint64_t one = 1;
    //向唤醒的channle中写入八个字节，唤醒等待线程，
    ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof one);
    if (n != sizeof one)
    {
        LOG<< "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

//唤醒channel的读处理函数
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = readn(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
    //pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
}

//函数的逻辑很简单：判断是否处于当前IO线程，是则执行这个函数，如果不是则将函数加入队列。
//在I/O线程中执行某个回调函数，该函数可以跨线程调用
void EventLoop::runInLoop(Functor&& cb)
{
    if (isInLoopThread()){
        //如果是当前I/O线程调用runInLoop，则同步调用cb
        cb();
    }
    else{
        //如果是其他线程调用runInloop，则异步地将cb添加到队列
        queueInLoop(std::move(cb));
    }      
}

void EventLoop::queueInLoop(Functor&& cb)
{
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }

    //调用queueInLoop的线程不是当前I/O线程需要唤醒
    //或者调用queueInLoop的线程是当前I/O线程，并且此时正在调用pendingfunctor，需要唤醒
    //只有当前I/O线程的事件回调queueLoop才不需要唤醒
    if (!isInLoopThread() || callingPendingFunctors_)
        wakeup();
}

//事件循环，该函数不能跨线程调用
//只能在创建对象相同的线程中调用
void EventLoop::loop()
{
    //是否还没开始循环
    assert(!looping_);
    //断言当前处于创建该对象的线程中
    assert(isInLoopThread());
    looping_ = true;//开始进行事件循环了
    quit_ = false;
    //LOG_TRACE << "EventLoop " << this << " start looping";
    std::vector<SP_Channel> ret;
    while (!quit_)
    {
        //cout << "doing" << endl;
        ret.clear();//活动通道清除
        ret = poller_->poll();//调用poll返回活动的通道
        eventHandling_ = true;//开始处理事件
        for (auto &it : ret)//遍历活动通道进行处理
            it->handleEvents();
        eventHandling_ = false;//结束处理事件
        // 执行pending Functors_中的任务回调
        // 这种设计使得IO线程也能执行一些计算任务，避免了IO线程在不忙时长期阻塞在IO multiplexing调用中
        doPendingFunctors();//让I/O线程也可以执行一些计算任务 
        poller_->handleExpired();
    }
    looping_ = false;//事件循环结束
}

/**
 * {
 *   MutexLockGuard lock(mutex_);
 *   functors.swap(pendingFunctors_);
 * }
 * 不是很简单的在临界区内依次调用Functor，而是把回调列表swap到functors中，这样一方面减少来临界区的长度
 * （意味着不会阻塞其它线程的queueInLoop())，另一个方面，也避免来死锁（因为Functor可能再次调用queueInLoop())
 * 
 * 由于doPendingFunctors调用的Functor可能再次调用queueInLoop(cb)，这是queueInLoop()就必须wakeup，
 * 否则新增的cb可能就不能及时调用来
 * 
 * muduo没有反复执行doPendingFunctors()直到PendingFunctors为空，这是有意的，
 * 否则I/O线程可能陷入死循环，无法处理I/O事件
 */
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (size_t i = 0; i < functors.size(); ++i)
        functors[i]();
    callingPendingFunctors_ = false;
}

//该函数可以跨线程调用
void EventLoop::quit()
{
    quit_ = true;
    //如果不是在io线程(当前线程)调用的，需要唤醒当前I/O线程
    if (!isInLoopThread())
    {
        wakeup();//唤醒当前I/O线程
    }
}
