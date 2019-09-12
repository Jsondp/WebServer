
#include "FileUtil.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

AppendFile::AppendFile(string filename)
:   fp_(fopen(filename.c_str(), "ae"))
{
    // 使用用户提供缓冲区
    setbuffer(fp_, buffer_, sizeof buffer_);
}


AppendFile::~AppendFile()
{
    fclose(fp_);//关闭文件指针
}

//写日志
void AppendFile::append(const char* logline, const size_t len)
{
    size_t n = this->write(logline, len);
    size_t remain = len - n;
    while (remain > 0)
    {
        size_t x = this->write(logline + n, remain);
        if (x == 0)
        {
            int err = ferror(fp_);
            if (err)
                fprintf(stderr, "AppendFile::append() failed !\n");
            break;
        }
        n += x;
        remain = len - n;
    }
}

void AppendFile::flush()
{
    fflush(fp_);
}


size_t AppendFile::write(const char* logline, size_t len)
{
    //非线程安全的写入
    //因为我们之前已经保证了线程安全所以我们这里使用非线程安全的函数可以提高效率
    return fwrite_unlocked(logline, 1, len, fp_);
    
}