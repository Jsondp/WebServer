
#include "CountDownLatch.h"

CountDownLatch::CountDownLatch(int count)
  : mutex_(),
    condition_(mutex_),//初始化条件变量，condition不负责mutex的生存周期的释放
    count_(count)//初始化计数器
{ }

void CountDownLatch::wait()
{
    MutexLockGuard lock(mutex_);
    while (count_ > 0)
        condition_.wait();
}

void CountDownLatch::countDown()
{
    MutexLockGuard lock(mutex_);
    --count_;
    if (count_ == 0)//如果count计数器为0，我们通知所有的等待线程
        condition_.notifyAll();
}