
#pragma once
#include <stdint.h>
/**
 * 对于编写多线程出现来说，我们最好不要再调用fork
 * 不要编写多线程多进程程序，要么用多进程，要么用多线程
 */
/* namespace CurrentThread
{
    //__thread修饰的变量是线程局部存储的
    //每个线程都有一份
   // __thread支持POD类型就是基本数据类型，不能是自定义类型
    __thread int t_cachedTid = 0;//线程真实Tid的缓存，因为如果每次都通过系统调用获取线程的Tid获取，效率会比较低
    __thread char t_tidString[32];//这是tid的字符串表示
    __thread int t_tidStringLength = 6;//tid的字符串长度
    __thread const char* t_threadName = "default";//设置线程的名字
} */

namespace CurrentThread
{
    // internal
    extern __thread int t_cachedTid;
    extern __thread char t_tidString[32];
    extern __thread int t_tidStringLength;
    extern __thread const char* t_threadName;
    void cacheTid();
    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }

    inline const char* tidString() // for logging
    {
        return t_tidString;
    }

    inline int tidStringLength() // for logging
    {
        return t_tidStringLength;
    }

    inline const char* name()
    {
        return t_threadName;
    }
}

