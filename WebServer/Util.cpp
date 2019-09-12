
#include "Util.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

const int MAX_BUFF = 4096;
ssize_t readn(int fd, void *buff, size_t n)
{
    size_t nleft = n;
    ssize_t nread = 0;
    ssize_t readSum = 0;
    char *ptr = (char*)buff;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR)
                nread = 0;
            else if (errno == EAGAIN)
            {
                return readSum;
            }
            else
            {
                return -1;
            }  
        }
        else if (nread == 0)
            break;
        readSum += nread;
        nleft -= nread;
        ptr += nread;
    }
    return readSum;
}

ssize_t readn(int fd, std::string &inBuffer, bool &zero)
{
    ssize_t nread = 0;
    ssize_t readSum = 0;
    while (true)
    {
        char buff[MAX_BUFF];
        if ((nread = read(fd, buff, MAX_BUFF)) < 0)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
            {
                return readSum;
            }  
            else
            {
                perror("read error");
                return -1;
            }
        }
        else if (nread == 0)
        {
            //printf("redsum = %d\n", readSum);
            zero = true;
            break;
        }
        //printf("before inBuffer.size() = %d\n", inBuffer.size());
        //printf("nread = %d\n", nread);
        readSum += nread;
        //buff += nread;
        inBuffer += std::string(buff, buff + nread);
        //printf("after inBuffer.size() = %d\n", inBuffer.size());
    }
    return readSum;
}


ssize_t readn(int fd, std::string &inBuffer)
{
    ssize_t nread = 0;
    ssize_t readSum = 0;
    while (true)
    {
        char buff[MAX_BUFF];
        if ((nread = read(fd, buff, MAX_BUFF)) < 0)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
            {
                return readSum;
            }  
            else
            {
                perror("read error");
                return -1;
            }
        }
        else if (nread == 0)
        {
            //printf("redsum = %d\n", readSum);
            break;
        }
        //printf("before inBuffer.size() = %d\n", inBuffer.size());
        //printf("nread = %d\n", nread);
        readSum += nread;
        //buff += nread;
        inBuffer += std::string(buff, buff + nread);
        //printf("after inBuffer.size() = %d\n", inBuffer.size());
    }
    return readSum;
}


ssize_t writen(int fd, void *buff, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten = 0;
    ssize_t writeSum = 0;
    char *ptr = (char*)buff;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0)
            {
                if (errno == EINTR)
                {
                    nwritten = 0;
                    continue;
                }
                else if (errno == EAGAIN)
                {
                    return writeSum;
                }
                else
                    return -1;
            }
        }
        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return writeSum;
}

ssize_t writen(int fd, std::string &sbuff)
{
    size_t nleft = sbuff.size();
    ssize_t nwritten = 0;
    ssize_t writeSum = 0;
    const char *ptr = sbuff.c_str();
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0)
            {
                if (errno == EINTR)
                {
                    nwritten = 0;
                    continue;
                }
                else if (errno == EAGAIN)
                    break;
                else
                    return -1;
            }
        }
        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    if (writeSum == static_cast<int>(sbuff.size()))
        sbuff.clear();
    else
        sbuff = sbuff.substr(writeSum);
    return writeSum;
}

/**
 * 捕获SIGPIPE信号，设置为忽略，防止出现该信号整个程序被结束
 */
void handle_for_sigpipe()
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL))
        return;
}

/**
 * 设置监听文件描述符为非阻塞状态
 */
int setSocketNonBlocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    if(flag == -1)
        return -1;

    flag |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flag) == -1)
        return -1;
    return 0;
}
/**
 * 关闭Nagle算法防止TCP粘包
 */
void setSocketNodelay(int fd) 
{
    int enable = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));
}
/**
 * 设置close的延迟返回
 * 在默认情况下,当调用close关闭socke的使用,close会立即返回,
 * 但是,如果send buffer中还有数据,系统会试着先把send buffer中的数据发送出去,然后close才返回.
 * SO_LINGER选项则是用来修改这种默认操作的.
 * 
 * 当l_onoff被设置为0的时候,将会关闭SO_LINGER选项,
 * 即TCP或则SCTP保持默认操作:close立即返回.l_linger值被忽略.
 * 
 * l_lineoff值非0，0 = l_linger
 * 当调用close的时候,TCP连接会立即断开.send buffer中未被发送的数据将被丢弃,并向对方发送一个RST信息.
 * 值得注意的是，由于这种方式，是非正常的4中握手方式结束TCP链接，所以，TCP连接将不会进入TIME_WAIT状态，
 * 这样会导致新建立的可能和就连接的数据造成混乱。
 */
void setSocketNoLinger(int fd) 
{
    struct linger linger_;
    linger_.l_onoff = 1;
    linger_.l_linger = 30;
    setsockopt(fd, SOL_SOCKET, SO_LINGER,(const char *) &linger_, sizeof(linger_));
}

/**
 * 关闭写操作
 * 
 * shutdown：SHUT_RD：read没有影响，只是返回0；
    关闭连接的读这一半，进程不能再对这样的套接字调用任何读操作；
    在套接字上不能再发出接收请求，进程仍可往套接字发送数据，套接字接收缓冲区中所有数据被丢弃，
    此套接字再接收到的任何数据由TCP丢弃，对套接字发送缓冲区没有任何影响；
    shutdown：SHUT_WR   ： 
        但是在调用shutdown关闭写之后不能再调用write方法，会引起错误终止程序，
        这里和上面的关闭读有差别，其实是在已发送FIN包后write该socket描述符会引发EPIPE/SIGPIPE。
        关闭连接的写这一半，进程不能再对这样的套接字调用任何写操作；
        在套接字上不能再发出发送请求，进程仍可从套接字接收数据，套接字发送缓冲区中的内容被发送到对端，
        后跟正常的TCP连接终止序列（即发送FIN），对套接字接收缓冲区无任何影响；
 */
void shutDownWR(int fd)
{
    shutdown(fd, SHUT_WR);//关闭写一半
    //printf("shutdown\n");
}

/**
 * 为了更好的理解 backlog 参数，我们必须认识到内核为任何一个给定的监听套接口维护两个队列：
    1、未完成连接队列（incomplete connection queue），每个这样的 SYN 分节对应其中一项：
    已由某个客户发出并到达服务器，而服务器正在等待完成相应的 TCP 三次握手过程。这些套接口处于 SYN_RCVD 状态。
    2、已完成连接队列（completed connection queue），每个已完成 TCP 三次握手过程的客户对应其中一项。
    这些套接口处于 ESTABLISHED 状态。

    当来自客户的 SYN 到达时，TCP 在未完成连接队列中创建一个新项，
    然后响应以三次握手的第二个分节：服务器的 SYN 响应，其中稍带对客户 SYN 的 ACK（即SYN+ACK），
    这一项一直保留在未完成连接队列中，直到三次握手的第三个分节（客户对服务器 SYN 的 ACK ）
    到达或者该项超时为止（曾经源自Berkeley的实现为这些未完成连接的项设置的超时值为75秒）。

    如果三次握手正常完成，该项就从未完成连接队列移到已完成连接队列的队尾。

    backlog 参数历史上被定义为上面两个队列的大小之和，大多数实现默认值为 5，
    当服务器把这个完成连接队列的某个连接取走后，这个队列的位置又空出一个，
    这样来回实现动态平衡，但在高并发 web 服务器中此值显然不够。
 */
int socket_bind_listen(int port)
{
    // 检查port值，取正确区间范围
    if (port < 0 || port > 65535)
        return -1;

    // 创建socket(IPv4 + TCP)，返回监听描述符
    int listen_fd = 0;
    if((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    // 端口复用，消除bind时"Address already in use"错误
    int optval = 1;
    if(setsockopt(listen_fd, SOL_SOCKET,  SO_REUSEADDR, &optval, sizeof(optval)) == -1)
        return -1;

    // 设置服务器IP和Port，和监听描述副绑定
    struct sockaddr_in server_addr;
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);
    if(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        return -1;

    // 开始监听，最大等待队列长为LISTENQ
    //2048为未完成连接队列和已完成连接队列的和
    if(listen(listen_fd, 2048) == -1)
        return -1;

    // 无效监听描述符
    if(listen_fd == -1)
    {
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}