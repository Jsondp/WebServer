
#include "Thread.h"
#include "CurrentThread.h"
#include <memory>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <stdint.h>
#include <assert.h>

#include <iostream>
using namespace std;

/**
 * 对于编写多线程出现来说，我们最好不要再调用fork
 * 不要编写多线程多进程程序，要么用多进程，要么用多线程
 */
namespace CurrentThread
{
    //__thread修饰的变量是线程局部存储的
    //每个线程都有一份
   // __thread支持POD类型就是基本数据类型，不能是自定义类型
    __thread int t_cachedTid = 0;//线程真实Tid的缓存，因为如果每次都通过系统调用获取线程的Tid获取，效率会比较低
    __thread char t_tidString[32];//这是tid的字符串表示
    __thread int t_tidStringLength = 6;//tid的字符串长度
    __thread const char* t_threadName = "default";//设置线程的名字
}

//调用系统调用获取tid
pid_t gettid()
{
    return static_cast<pid_t>(::syscall(SYS_gettid));
}

//缓存线程的tid
void CurrentThread::cacheTid()
{
    if (t_cachedTid == 0)//如果我们还没有缓存过
    {
        t_cachedTid = gettid();//调用系统调用缓存Tid
        t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
    }
}

// 为了在线程中保留name,tid这些数据
struct ThreadData
{
    typedef Thread::ThreadFunc ThreadFunc;
    ThreadFunc func_; //线程回调函数
    string name_;
    pid_t* tid_;
    CountDownLatch* latch_;

    ThreadData(const ThreadFunc &func, const string& name, pid_t *tid, CountDownLatch *latch)
    :   func_(func),
        name_(name),
        tid_(tid),
        latch_(latch)
    { }


    void runInThread()
    {
        *tid_ = CurrentThread::tid();
        tid_ = NULL;
        //条件变量1变为0,线程开始工作
        latch_->countDown();
        latch_ = NULL;

        CurrentThread::t_threadName = name_.empty() ? "Thread" : name_.c_str();//缓冲线程的名称
        //用于线程重命名，便于命令ps -ef查看。
        prctl(PR_SET_NAME, CurrentThread::t_threadName);

        func_();//运行线程运行函数
        //标志当前线程为结束状态
        CurrentThread::t_threadName = "finished";
    }
};

void *startThread(void* obj)
{
    ThreadData* data = static_cast<ThreadData*>(obj);
    data->runInThread();
    delete data;
    return NULL;
}


Thread::Thread(const ThreadFunc &func, const string &n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(0),
    func_(func),
    name_(n),
    latch_(1)
{
    setDefaultName();
}


/**
 * pthread_join()和pthread_detach()都是防止现成资源泄露的途径，join()会阻塞等待。
 * 这个析构函数是线程安全的。析构时确认thread没有join，才会执行析构。即线程的析构不会等待线程结束
 * 如果thread对象的生命周期长于线程，那么可以通过join等待线程结束。
 * 否则thread对象析构时会自动detach线程，防止资源泄露
 */
Thread::~Thread()    
{                                    
    //如果没有join，就detach，如果用过了，就不用了。
    if (started_ && !joined_)   
        pthread_detach(pthreadId_);
}

/**
 * 设置线程的名字
 */
void Thread::setDefaultName()
{
    if (name_.empty())
    {
        char buf[32];
        snprintf(buf, sizeof buf, "Thread");
        name_ = buf;
    }
}

//线程启动函数,调用pthread_create创建线程
void Thread::start()
{
    assert(!started_);//确保线程没有启动
    started_ = true;//设置标记，线程已经启动
     //data存放了线程真正要执行的函数,记为func,线程id,线程name等信息 
    ThreadData* data = new ThreadData(func_, name_, &tid_, &latch_);
    //创建线程:线程函数为startThread
    //如果线程创建成功，条件变量就开始等待，否则还原初始化的值
    if (pthread_create(&pthreadId_, NULL, &startThread, data))
    {
        started_ = false;//创建线程失败,设置标记线程未启动
        delete data;
    }
    else
    {
        latch_.wait();//等待线程运行了线程函数才退出当前函数
        assert(tid_ > 0);
    }
}

int Thread::join()
{
    assert(started_);
    assert(!joined_);
    joined_ = true;
    //阻塞等待线程结束
    return pthread_join(pthreadId_, NULL);
}