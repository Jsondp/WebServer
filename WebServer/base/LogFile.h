
#pragma once
#include "FileUtil.h"
#include "MutexLock.h"
#include "noncopyable.h"
#include <memory>
#include <string>

// TODO 提供自动归档功能
class LogFile : noncopyable
{
public:
    // 每被append flushEveryN次，flush一下，会往文件写，只不过，文件也是带缓冲区的
    LogFile(const std::string& basename, int flushEveryN = 1024);
    ~LogFile();

    void append(const char* logline, int len);//往缓冲区添加长度为len的logline数据
    void flush();//把日志保存到磁盘中
    bool rollFile();//日志滚动

private:
    void append_unlocked(const char* logline, int len);

    const std::string basename_;//日志文件的basename，basename就是最基本的名字
    const int flushEveryN_;//日志写入间隔时间

    int count_;//记录append的次数
    std::unique_ptr<MutexLock> mutex_;
    std::unique_ptr<AppendFile> file_;
};