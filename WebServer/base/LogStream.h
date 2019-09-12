
#pragma once
#include "noncopyable.h"
#include <assert.h>
#include <string.h>
#include <string>


class AsyncLogging;
const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000 * 1000;

//非类型参数
template<int SIZE> 
class FixedBuffer: noncopyable  //缓冲区
{
public:
    FixedBuffer()
    :   cur_(data_)
    { }

    ~FixedBuffer()
    { }

    void append(const char* buf, size_t len)//添加长度为len的buf数据到缓冲区中
    {
        if (avail() > static_cast<int>(len))//当前有可用的空间就将数据添加到缓冲区当中
        {
            memcpy(cur_, buf, len);
            cur_ += len;
        }
    }

    const char* data() const { return data_; }//返回当前缓冲区的数据
    int length() const { return static_cast<int>(cur_ - data_); }//获取当前缓冲区的使用的大小

    char* current() { return cur_; }//返回当前缓冲区的末尾的指针
    int avail() const { return static_cast<int>(end() - cur_); }//当前可用的空间
    void add(size_t len) { cur_ += len; }//

    void reset() { cur_ = data_; }//重置这块缓冲区
    void bzero() { memset(data_, 0, sizeof data_); }//初始化这块缓冲区


private:
    const char* end() const { return data_ + sizeof data_; }//返回缓冲区的结尾指针

    char data_[SIZE];//缓冲区的容量
    char* cur_;//指向当前缓冲区的结尾
};



class LogStream : noncopyable
{
public:
    //FixedBuffer在上面定义了
    typedef FixedBuffer<kSmallBuffer> Buffer;//创建一个缓冲区容量为4000的缓冲区

    //重载<<运算符，插入到缓冲区当中
    LogStream& operator<<(bool v)//输出到
    {
        // v 为真返回“1”为假返回0，插入一个字符
        buffer_.append(v ? "1" : "0", 1);
        return *this;
    }
    //重载<<运算符，插入到缓冲区当中
    LogStream& operator<<(short);
    LogStream& operator<<(unsigned short);
    LogStream& operator<<(int);
    LogStream& operator<<(unsigned int);
    LogStream& operator<<(long);
    LogStream& operator<<(unsigned long);
    LogStream& operator<<(long long);
    LogStream& operator<<(unsigned long long);

    LogStream& operator<<(const void*);

    LogStream& operator<<(float v)
    {
        *this << static_cast<double>(v);
        return *this;
    }
    LogStream& operator<<(double);
    LogStream& operator<<(long double);

    LogStream& operator<<(char v)
    {
        buffer_.append(&v, 1);
        return *this;
    }

    LogStream& operator<<(const char* str)
    {
        if (str)
            buffer_.append(str, strlen(str));
        else
            buffer_.append("(null)", 6);
        return *this;
    }

    LogStream& operator<<(const unsigned char* str)
    {
        return operator<<(reinterpret_cast<const char*>(str));
    }

    LogStream& operator<<(const std::string& v)
    {
        buffer_.append(v.c_str(), v.size());
        return *this;
    }

    void append(const char* data, int len) { buffer_.append(data, len); }
    const Buffer& buffer() const { return buffer_; }
    void resetBuffer() { buffer_.reset(); }

private:
    void staticCheck();

    template<typename T>
    void formatInteger(T);//成员模版 //把整形转字符串

    Buffer buffer_;

    static const int kMaxNumericSize = 32;
};