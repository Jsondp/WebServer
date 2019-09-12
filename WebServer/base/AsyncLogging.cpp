
#include "AsyncLogging.h"
#include "LogFile.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <functional>


AsyncLogging::AsyncLogging(std::string logFileName_,int flushInterval)
  : flushInterval_(flushInterval),
    running_(false),
    basename_(logFileName_),
    thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),//预先准备了两块缓冲区currentBuffer_,nextBuffer_
    nextBuffer_(new Buffer),
    buffers_(),
    latch_(1)
{
    assert(logFileName_.size() > 1);
    //将清空缓冲区当中的内容
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    //缓冲区列表预留16个位置的空间
    buffers_.reserve(16);
}
//前端线程调用append 往缓冲区当前写入数据
void AsyncLogging::append(const char* logline, int len)
{
    //因为前端有多个线程对缓冲区进行写入所以要进行加锁保护缓冲区
    MutexLockGuard lock(mutex_);
    if (currentBuffer_->avail() > len)//判断当前缓冲区是否已经满了，如果未满则加入
        //当前缓冲区未满，将数据追加到末尾
        currentBuffer_->append(logline, len);
    else
    {
        //当前缓冲区已满，将当前缓冲区添加到待写入文件的已填满的缓冲区中
        buffers_.push_back(currentBuffer_);

        currentBuffer_.reset();
        if (nextBuffer_){//判断nextBuffer是否为空
            //将当前缓冲区设置为预备缓冲区
            //移动语义将nextBuffer的缓冲移动到currentBuffer的缓冲区，然后nextBuffer不再有指向了
            currentBuffer_ = std::move(nextBuffer_);
        }        
        else{
            //nextBuffer没有指向为空的情况
            //这种情况极少发生，前端写入速度太快，一下子把两块缓冲区都写完
            //那么，只好分配一块新的缓冲区
            currentBuffer_.reset(new Buffer);
        }
        currentBuffer_->append(logline, len);
        cond_.notify();//通知后端开始写入日志
    }
}

//后端日志线程
void AsyncLogging::threadFunc()
{
    assert(running_ == true);
    latch_.countDown();
    LogFile output(basename_);
    //准备两块缓冲区
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->bzero();
    newBuffer2->bzero();
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    while (running_)
    {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);
        assert(buffersToWrite.empty());

        {
            MutexLockGuard lock(mutex_);
            if (buffers_.empty())  // unusual usage!（注意，这里是一个非常规的用法）
            {
                //等待前端写满了一个或者多个buffer，或者一个超时时间到来
                cond_.waitForSeconds(flushInterval_);
            }
            //将当前缓冲区移入buffers
            buffers_.push_back(currentBuffer_);
            currentBuffer_.reset();
            //将空闲的newBuffer1置为当前缓冲区
            currentBuffer_ = std::move(newBuffer1);//移动语义没有涉及缓冲区的拷贝操作，只是交换指针而已 
            //buffers_和buffersToWrite交换，这样后面的代码可以在临界区之外安全地访问buffersToWrite
            buffersToWrite.swap(buffers_);//交换后这样前端的日志线程和后端的日志线程可以并发
            if (!nextBuffer_)
            {
                //确保前端始终又一个预备buffer可供调配
                //减少前端临界区分配内存的概率，缩短前端临界区长度。
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        assert(!buffersToWrite.empty());
        //消息堆积
        //前端陷入死循环，拼命发送日志消息，超过后端的处理能力，这就是典型的生产速度
        //超过消费速度的问题，会造成数据在内存中堆积，严重时引发性能问题（可用内存不足）
        //或者程序崩溃（分配内存失败）
        if (buffersToWrite.size() > 25)
        {
            //char buf[256];
            // snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
            //          Timestamp::now().toFormattedString().c_str(),
            //          buffersToWrite.size()-2);
            //fputs(buf, stderr);
            //output.append(buf, static_cast<int>(strlen(buf)));
            //丢掉多余的日志，以腾出内存，仅保留两块缓冲区
            buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end());
        }

        //buffersToWrite的所有日志写到文件当中
        for (size_t i = 0; i < buffersToWrite.size(); ++i)
        {
            // FIXME: use unbuffered stdio FILE ? or use ::writev ?
            output.append(buffersToWrite[i]->data(), buffersToWrite[i]->length());
        }

        if (buffersToWrite.size() > 2)
        {
            // drop non-bzero-ed buffers, avoid trashing
            //仅保留两个buffer，用于newBuffer1 与 newBuffer2
            buffersToWrite.resize(2);
        }

        //如果newBuffer1为空，向buffersToWrite中的获取一块缓冲区
        if (!newBuffer1)
        {
            assert(!buffersToWrite.empty());
            newBuffer1 = buffersToWrite.back();
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        //如果newBuffer2为空，向buffersToWrite中的获取一块缓冲区
        if (!newBuffer2)
        {
            assert(!buffersToWrite.empty());
            newBuffer2 = buffersToWrite.back();
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        //buffersToWrite有多余部分把它清空，释放掉
        buffersToWrite.clear();
        output.flush();//把缓冲区中的数据写入到文件当中
    }
    output.flush();
}
