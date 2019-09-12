
#pragma once
#include "Timer.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <sys/epoll.h>
#include <functional>
#include <sys/epoll.h>


class EventLoop;
class HttpData;


class Channel
{
private:
    typedef std::function<void()> CallBack;//事件的回调处理
    EventLoop *loop_;//所属的Eventloop
    int fd_;//文件描述符，但不负责关闭文件描述符
    __uint32_t events_;//关注事件
    __uint32_t revents_;//poll/epoll返回的事件
    __uint32_t lastEvents_;//上一次的关注处理事件

    /**
     * 当连接到来，创建一个TcpConnect对象，立刻用shared_ptr来管理，引用计数为 1
     * 在channel中维护一个weak_ptr（holder_），将这个share_ptr对象赋值给（holder_），引用计数仍为1
     * 当连接关闭，在handleEvent，将（holder_）提升，得到一个shared_ptr对象，引用计数就变成 2
     */
    // 方便找到上层持有该Channel的对象
    std::weak_ptr<HttpData> holder_;//方便判断该连接是否存在，如果不能提升说明连接关闭

private:
    int parse_URI();
    int parse_Headers();
    int analysisRequest();

    //回调函数的注册
    CallBack readHandler_;
    CallBack writeHandler_;
    CallBack errorHandler_;
    CallBack connHandler_;

public:
    Channel(EventLoop *loop);
    Channel(EventLoop *loop, int fd);
    ~Channel();
    int getFd();
    void setFd(int fd);

    void setHolder(std::shared_ptr<HttpData> holder)
    {
        holder_ = holder;
    }

    std::shared_ptr<HttpData> getHolder()
    {
        std::shared_ptr<HttpData> ret(holder_.lock());//获取上层对象
        return ret;
    }

    //注册处理事件
    void setReadHandler(CallBack &&readHandler)
    {
        readHandler_ = readHandler;
    }
    void setWriteHandler(CallBack &&writeHandler)
    {
        writeHandler_ = writeHandler;
    }
    void setErrorHandler(CallBack &&errorHandler)
    {
        errorHandler_ = errorHandler;
    }
    void setConnHandler(CallBack &&connHandler)
    {
        connHandler_ = connHandler;
    }

    //查看产生了什么事件
    //EPOLLHUP：对等端关闭了连接或者showdown了我们这边都会收到这个事件
    //我们把EPOLLHUP的处理为读事件
    /**
     *  events这个参数是一个字节的掩码构成的。下面是可以用的事件：
        EPOLLIN - 当关联的文件可以执行 read ()操作时。
        EPOLLOUT - 当关联的文件可以执行 write ()操作时。
        EPOLLRDHUP - (从 linux 2.6.17 开始)当socket关闭的时候，
        或者半关闭写段的(当使用边缘触发的时候，这个标识在写一些测试代码去检测关闭的时候特别好用)
        EPOLLPRI - 当 read ()能够读取紧急数据的时候。
        EPOLLERR - 当关联的文件发生错误的时候，epoll_wait() 总是会等待这个事件，并不是需要必须设置的标识。
        EPOLLHUP - 当指定的文件描述符被挂起的时候。epoll_wait() 总是会等待这个事件，
                    并不是需要必须设置的标识。当socket从某一个地方读取数据的时候(管道或者socket),
                    这个事件只是标识出这个已经读取到最后了(EOF)。所有的有效数据已经被读取完毕了，
                    之后任何的读取都会返回0(EOF)。
        EPOLLET - 设置指定的文件描述符模式为边缘触发，默认的模式是水平触发。
        EPOLLONESHOT - (从 linux 2.6.17 开始)设置指定文件描述符为单次模式。
                    这意味着，在设置后只会有一次从epoll_wait() 中捕获到事件，
                    之后你必须要重新调用 epoll_ctl() 重新设置。
     */
   void handleEvents()
    {
        events_ = 0;
        if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
        {
            events_ = 0;
            return;
        }
        if (revents_ & EPOLLERR)
        {
            if (errorHandler_) errorHandler_();
            events_ = 0;
            return;
        }
        //EPOLLRDHUP当socket关闭的时候，或者半关闭写段的
        if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
        {
            handleRead();
        }
        if (revents_ & EPOLLOUT)
        {
            handleWrite();
        }
        handleConn();
    }
    void handleRead();
    void handleWrite();
    void handleError(int fd, int err_num, std::string short_msg);
    void handleConn();

    //设置 poll/epoll返回的事件
    void setRevents(__uint32_t ev)
    {
        revents_ = ev;
    }

    //设置poll/epoll关注的事件
    void setEvents(__uint32_t ev)
    {
        events_ = ev;
    }

    //获取poll/epoll关注的事件
    __uint32_t& getEvents()
    {
        return events_;
    }

    //如果当前关注的事件和之前关注的事件不一样，更新上一次的关注处理事件
    bool EqualAndUpdateLastEvents()
    {
        bool ret = (lastEvents_ == events_);
        lastEvents_ = events_;
        return ret;
    }

    //获取上一次的关注处理事件
    __uint32_t getLastEvents()
    {
        return lastEvents_;
    }
};

typedef std::shared_ptr<Channel> SP_Channel;