
#pragma once
#include "CountDownLatch.h"
#include "noncopyable.h"
#include <functional>
#include <memory>
#include <pthread.h>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

/**
 * 继承noncopyable的原因是不写的话，偶然情况下可能会写出来拷贝，导致不可预料的问题，比如指针
 */
class Thread : noncopyable
{
public:
    typedef std::function<void ()> ThreadFunc;
    explicit Thread(const ThreadFunc&, const std::string& name = std::string());
    ~Thread();
    void start();// 启动线程
    int join();
    bool started() const { return started_; }
    pid_t tid() const { return tid_; }//返回线程id
    const std::string& name() const { return name_; }//返回线程名

private:
    void setDefaultName();
    bool started_;//启动标识
    bool joined_;
    pthread_t pthreadId_;// pthread_t给pthraed_xxx函数使用
    pid_t tid_;// pid_t作为线程标识
    ThreadFunc func_;
    std::string name_;
    CountDownLatch latch_;
};