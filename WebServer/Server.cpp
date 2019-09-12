
#include "Server.h"
#include "base/Logging.h"
#include "Util.h"
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//I/O线程即EventLoop

/**
 * 初始化EventLoop，线程池的个数，端口
 */
Server::Server(EventLoop *loop, int threadNum, int port)//这里的threadNum代表要创建EventLoop的个数
:   loop_(loop),
    threadNum_(threadNum),
    //初始化EventLoopThreadPool的线程池个数，如果这里设置 4个，实际上有5个，因为要加上当前这一个主的I/O线程
    eventLoopThreadPool_(new EventLoopThreadPool(loop_, threadNum)),
    started_(false),
    acceptChannel_(new Channel(loop_)),
    port_(port),
    listenFd_(socket_bind_listen(port_))
{
    acceptChannel_->setFd(listenFd_);
    handle_for_sigpipe();
    if (setSocketNonBlocking(listenFd_) < 0)
    {
        perror("set socket non block failed");
        abort();
    }
}
//该函数多次调用是无害的
//该函数可以跨线程调用
void Server::start()
{
    eventLoopThreadPool_->start();//启动线程池的I/O线程即EventLoop
    //acceptChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    acceptChannel_->setEvents(EPOLLIN | EPOLLET);
    acceptChannel_->setReadHandler(bind(&Server::handNewConn, this));
    acceptChannel_->setConnHandler(bind(&Server::handThisConn, this));
    loop_->addToPoller(acceptChannel_, 0);
    started_ = true;
}

//当新的一个连接到来，就会回调当前函数
void Server::handNewConn()
{
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_addr_len = sizeof(client_addr);
    int accept_fd = 0;
    while((accept_fd = accept(listenFd_, (struct sockaddr*)&client_addr, &client_addr_len)) > 0)
    {
        //按照轮询的方式选择一个EventLoop，也就是选择一个EventLoop所对应的线程来进行处理该连接
        EventLoop *loop = eventLoopThreadPool_->getNextLoop();
        LOG << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port);
        // cout << "new connection" << endl;
        // cout << inet_ntoa(client_addr.sin_addr) << endl;
        // cout << ntohs(client_addr.sin_port) << endl;
        /*
        // TCP的保活机制默认是关闭的
        int optval = 0;
        socklen_t len_optval = 4;
        getsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
        cout << "optval ==" << optval << endl;
        */
        // 限制服务器的最大并发连接数
        if (accept_fd >= MAXFDS)
        {
            close(accept_fd);
            continue;
        }
        // 设为非阻塞模式
        if (setSocketNonBlocking(accept_fd) < 0)
        {
            LOG << "Set non block failed!";
            //perror("Set non block failed!");
            return;
        }
        /**
         * 因为发送的内容很少，为避免发送可能的延迟，关闭Nagle算法
         */
        setSocketNodelay(accept_fd);
        //setSocketNoLinger(accept_fd);
        //选择轮询出来的EventLoop来处理当前请求
        shared_ptr<HttpData> req_info(new HttpData(loop, accept_fd));
        //req_info当前连接的信息
        req_info->getChannel()->setHolder(req_info);
        //把当前连接放到当前的EventLoop处理队列里面
        loop->queueInLoop(std::bind(&HttpData::newEvent, req_info));
    }
    acceptChannel_->setEvents(EPOLLIN | EPOLLET);//重新设置监听
}