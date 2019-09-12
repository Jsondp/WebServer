#pragma once
#include "Timer.h"
#include <string>
#include <unordered_map>
#include <map>
#include <memory>
#include <sys/epoll.h>
#include <functional>
#include <unistd.h>

class EventLoop;
class TimerNode;
class Channel;

/**
 * HTTP的请求状态
 */
enum ProcessState
{
    STATE_PARSE_URI = 1,
    STATE_PARSE_HEADERS,//解析头状态
    STATE_RECV_BODY,//接受body状态
    STATE_ANALYSIS,//解析状态
    STATE_FINISH//完成状态
};

/**
    统一资源标志符URI就是在某一规则下能把一个资源独一无二地标识出来。拿人做例子，
    假设这个世界上所有人的名字都不能重复，那么名字就是URI的一个实例，
    通过名字这个字符串就可以标识出唯一的一个人。现实当中名字当然是会重复的，
    所以身份证号才是URI，通过身份证号能让我们能且仅能确定一个人。那统一资源定位符URL是什么呢。
    也拿人做例子然后跟HTTP的URL做类比，
    就可以有：动物住址协议://地球/中国/浙江省/杭州市/西湖区/某大学/14号宿舍楼/525号寝/张三.人可以看到，
    这个字符串同样标识出了唯一的一个人，起到了URI的作用，所以URL是URI的子集。
    URL是以描述人的位置来唯一确定一个人的。在上文我们用身份证号也可以唯一确定一个人。
    对于这个在杭州的张三，我们也可以用：身份证号：123456789来标识他。
    所以不论是用定位的方式还是用编号的方式，我们都可以唯一确定一个人，
    都是URl的一种实现，而URL就是用定位的方式实现的URI。回到Web上，假设所有的Html文档都有唯一的编号，
    记作html:xxxxx，xxxxx是一串数字，即Html文档的身份证号码，这个能唯一标识一个Html文档，
    那么这个号码就是一个URI。而URL则通过描述是哪个主机上哪个路径上的文件来唯一确定一个资源，
    也就是定位的方式来实现的URI。对于现在网址我更倾向于叫它URL，毕竟它提供了资源的位置信息，
    如果有一天网址通过号码来标识变成了http://741236985.html，那感觉叫成URI更为合适，
    不过这样子的话还得想办法找到这个资源咯…
 * 大白话，就是URI是抽象的定义，不管用什么方法表示，
 * 只要能定位一个资源，就叫URI，本来设想的的使用两种方法定位：1，URL，用地址定位；2，URN 用名称定位。
 */
enum URIState
{
    PARSE_URI_AGAIN = 1,
    PARSE_URI_ERROR,
    PARSE_URI_SUCCESS,
};

enum HeaderState
{
    PARSE_HEADER_SUCCESS = 1,
    PARSE_HEADER_AGAIN,
    PARSE_HEADER_ERROR
};

enum AnalysisState
{
    ANALYSIS_SUCCESS = 1,
    ANALYSIS_ERROR
};

enum ParseState
{
    H_START = 0,
    H_KEY,
    H_COLON,
    H_SPACES_AFTER_COLON,
    H_VALUE,
    H_CR,
    H_LF,
    H_END_CR,
    H_END_LF
};

enum ConnectionState
{
    H_CONNECTED = 0,
    H_DISCONNECTING,
    H_DISCONNECTED    
};

/**
 * HTTP请求的方法
 */
enum HttpMethod
{
    METHOD_POST = 1,
    METHOD_GET,
    METHOD_HEAD
};

/**
 * HTTP协议版本
 */
enum HttpVersion
{
    HTTP_10 = 1,
    HTTP_11
};

/**
 * 文档媒体类型（MIME）
 */
class MimeType
{
private:
    static void init();
    static std::unordered_map<std::string, std::string> mime;
    MimeType();
    MimeType(const MimeType &m);

public:
    static std::string getMime(const std::string &suffix);

private:
    static pthread_once_t once_control;
};


class HttpData: public std::enable_shared_from_this<HttpData>
{
public:
    HttpData(EventLoop *loop, int connfd);
    ~HttpData() { close(fd_); }
    void reset();
    void seperateTimer();
    void linkTimer(std::shared_ptr<TimerNode> mtimer)
    {
        // shared_ptr重载了bool, 但weak_ptr没有
        timer_ = mtimer; 
    }
    std::shared_ptr<Channel> getChannel() { return channel_; }
    EventLoop *getLoop() { return loop_; }
    void handleClose();
    void newEvent();

private:
    EventLoop *loop_;//关联的EventLoop
    std::shared_ptr<Channel> channel_;//当前连接关联的channel
    int fd_;//该连接对应文件描述符
    std::string inBuffer_;//读缓冲区
    std::string outBuffer_;//写缓冲区
    bool error_;
    ConnectionState connectionState_;//连接的状态

    HttpMethod method_;//请求方法
    HttpVersion HTTPVersion_;//协议版本1.0/1.1
    std::string fileName_;//要请求的文件名
    std::string path_;//请求路径
    int nowReadPos_;//当前读到的下标
    ProcessState state_;//HTTP的请求状态
    ParseState hState_;//请求解析状态
    bool keepAlive_;//是否开启keepAlive_
    std::map<std::string, std::string> headers_;//header列表
    
    std::weak_ptr<TimerNode> timer_;//用于关联超时时间

    /**
     * 在处理HTTP请求（即调用onRequest）的过程中回调此函数，对请求进行具体的操作
     */
    void handleRead();
    void handleWrite();
    void handleConn();
    void handleError(int fd, int err_num, std::string short_msg);
    URIState parseURI();
    HeaderState parseHeaders();
    AnalysisState analysisRequest();
};