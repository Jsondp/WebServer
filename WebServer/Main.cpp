
#include "EventLoop.h"
#include "Server.h"
#include "base/Logging.h"
#include <getopt.h>
#include <string>

int main(int argc, char *argv[])
{
    int threadNum = 4;
    int port = 80;
    std::string logPath = "./WebServer.log";

    // parse args
    int opt;
    const char *str = "t:l:p:";
    while ((opt = getopt(argc, argv, str))!= -1)
    {
        switch (opt)
        {
            //设置线程的个数
            case 't':
            {
                threadNum = atoi(optarg);
                break;
            }
            //设置log日志的存放路径
            case 'l':
            {
                logPath = optarg;
                if (logPath.size() < 2 || optarg[0] != '/')
                {
                    printf("logPath should start with \"/\"\n");
                    abort();
                }
                break;
            }
            //设置监听的端口
            case 'p':
            {
                port = atoi(optarg);
                break;
            }
            default: break;
        }
    }
    Logger::setLogFileName(logPath);
    // STL库在多线程上应用
    #ifndef _PTHREADS
        LOG << "_PTHREADS is not defined !";
    #endif
    //创建主的线程，用于处理accept
    EventLoop mainLoop;
    //初始化线程池的线程个数，监听的端口
    //创建Server，绑定主线程
    Server myHTTPServer(&mainLoop, threadNum, port);//HTTP服务器监听的端口 port
    myHTTPServer.start();//服务器启动
    mainLoop.loop();//进入事件循环，实际上这里已经有threadNum+1个事件循环EventLoop
    return 0;
}
