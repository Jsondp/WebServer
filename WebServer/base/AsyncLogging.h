
#pragma once
#include "CountDownLatch.h"
#include "MutexLock.h"
#include "Thread.h"
#include "LogStream.h"
#include "noncopyable.h"
#include <functional>
#include <string>
#include <vector>
/*
日志的写入不是实时的
多个线程对同一个文件进行写入，效率可能不如单个线程对同一个文件写入效率高

异步日志（生产者消费者模型的一种应用）
前端线程即业务线程写日志不会阻塞，因为不用访问磁盘，只会写到缓冲区然后由后台线程来写入到磁盘
前端的线程 生产者 可以有多个
后端的线程 消费者 只有一个

生产者
p(semfull)
    p(mutex)
    往队列中添加一条日志消息
    x(mutex)
v(semempty)

消费者
p(semempty)
    p(mutex)
    从队列中取出所有日志消息，写入到文件中
    x(mutex)
v(semfull)

上面的机制如果生产者写入一个次日志就会通知消费者把日志写到文件中，写操作回比较频繁，效率比较低

多缓冲机制 multiple buffering


 */
//异步日志类 非阻塞日志
class AsyncLogging : noncopyable
{
public:
    AsyncLogging(const std::string basename, int flushInterval = 2);
    ~AsyncLogging()
    {
        if (running_)
            stop();
    }
    //供前端生产者线程调用（日志数据写到缓冲区）
    void append(const char* logline, int len);

    void start()
    {
        running_ = true;
        thread_.start();//日志线程启动
        latch_.wait();//等待线程已经启动了，才往下走
    }

    void stop()
    {
        running_ = false;
        cond_.notify();
        thread_.join();
    }


private:
    void threadFunc();//供后端消费者线程调用（将数据写到日志文件）
    typedef FixedBuffer<kLargeBuffer> Buffer;//缓冲区类型，kLargeBuffer为缓冲区的大小
    /* 
        ptr_vector负责动态内存的生命期
        vector不负责内部动态内存的生命期
    */
    typedef std::vector<std::shared_ptr<Buffer>> BufferVector;//缓冲区列表类型
    //可以理解为Buffer的智能指针，能管理Buffer的生存期，类似于C++11的unique_ptr，具备移动语义
    //两个unique_ptr不能指向一个对象，不能进行复制操作只能进行移动操作
    /*
        unique_ptr不能进行两指针的赋值
        unique p1;
        unique p2;
        p1 = p2;//错误的，不能进行复制，只能进行移动语义
        p1 = std::move(p2);//正确，只能进行移动语义的操作
     */
    typedef std::shared_ptr<Buffer> BufferPtr;
    
    const int flushInterval_;//超时时间，在flushInterval内，缓冲区没写满，仍将缓冲区中的数据写到文件中
    bool running_;
    std::string basename_;//日志文件的basename，即文件名
    Thread thread_;//日志线程
    MutexLock mutex_;//互斥锁
    Condition cond_;//条件变量
    BufferPtr currentBuffer_;//当前缓冲区
    BufferPtr nextBuffer_;//预备缓冲区
    BufferVector buffers_;//待写入文件的已填满的缓冲区
    CountDownLatch latch_;//用于等待线程启动
};