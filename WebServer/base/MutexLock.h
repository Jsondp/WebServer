
#pragma once
#include "noncopyable.h"
#include <pthread.h>
#include <cstdio>

class MutexLock: noncopyable
{
public:
    MutexLock()
    {
        //创建一个锁;
        pthread_mutex_init(&mutex, NULL);
    }
    ~MutexLock()
    {
        //尝试去lock，lock不到时永久等待;
        //pthread_mutex_lock(&mutex);
        //销毁锁;
        pthread_mutex_destroy(&mutex);
    }
    void lock()
    {
        pthread_mutex_lock(&mutex);
    }
    void unlock()
    {
        //释放一个锁;
        pthread_mutex_unlock(&mutex);
    }
    pthread_mutex_t *get()
    {
        return &mutex;
    }
private:
    pthread_mutex_t mutex;//定义一个锁变量

// 友元类不受访问权限影响
private:
    friend class Condition;
};

/**
 * MutexLockGuard可以防止我们忘记解锁或者程序退出没有解锁造成死锁的情况
 */
class MutexLockGuard: noncopyable
{
public:
    explicit MutexLockGuard(MutexLock &_mutex):
    mutex(_mutex)
    {
        mutex.lock();
    }
    ~MutexLockGuard()
    {
        mutex.unlock();
    }
private:
    MutexLock &mutex;//mutex是引用代表mutex的MutexLock生存周期不由MutexLockGuard管理，退出不会被释放
};