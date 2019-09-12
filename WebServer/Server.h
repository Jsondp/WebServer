
#pragma once
#include "EventLoop.h"
#include "Channel.h"
#include "EventLoopThreadPool.h"
#include <memory>


class Server
{
public:
    Server(EventLoop *loop, int threadNum, int port);
    ~Server() { }
    EventLoop* getLoop() const { return loop_; }
    void start();
    void handNewConn();
    void handThisConn() { loop_->updatePoller(acceptChannel_); }

private:
    EventLoop *loop_;//关联的EventLoop
    int threadNum_;//线程池中的个数
    //服务器包含一个eventLoopThreadPool_对象
    std::unique_ptr<EventLoopThreadPool> eventLoopThreadPool_;
    bool started_;//是否已经开启标志
    std::shared_ptr<Channel> acceptChannel_;//用来accept的channel
    int port_;//服务端口
    int listenFd_;//监听套接字
    static const int MAXFDS = 100000;
};