
#pragma once
#include "noncopyable.h"
#include <string>

class AppendFile : noncopyable
{
public:
    explicit AppendFile(std::string filename);//日志文件名filename
    ~AppendFile();
    // append 会向文件写
    void append(const char *logline, const size_t len);
    void flush();

private:
    size_t write(const char *logline, size_t len);
    FILE* fp_;//文件指针
    char buffer_[64*1024];//文件指针的缓冲区
};