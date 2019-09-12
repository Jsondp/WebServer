
#pragma once
#include "Condition.h"
#include "MutexLock.h"
#include "noncopyable.h"

// CountDownLatch的主要作用是确保Thread中传进去的func真的启动了以后
// 外层的start才返回
/**
 * 既可以用于所有子线程等待主线程发起“起跑”，即主线程通知所有的子线程执行
 * 也可以用于主线程等待子线程初始化完毕才开始工作
 */
class CountDownLatch : noncopyable
{
public:
    explicit CountDownLatch(int count);
    void wait();
    void countDown();

private:
    mutable MutexLock mutex_;//const的常函数可改变mutable修饰的变量
    Condition condition_;
    int count_;//计数器
};