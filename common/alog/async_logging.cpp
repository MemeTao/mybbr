#include <rtc/common/alog/async_logging.h>
//#include <unistd.h>
#include <functional>
#include <cassert>
#include <rtc/common/alog/log_file.h>


namespace rtc{
namespace common{
namespace alog{
using std::string;
using std::unique_ptr;
using std::shared_ptr;

AsyncLogging::AsyncLogging(const string& file_path,uint64_t rollsize)
    :log_file_path_ (file_path),
    roll_size_( rollsize),
    cur_buffer_(new Buffer),
    next_buffer_(new Buffer),
    running_(true)
{
    buffers_.reserve(16); 
    thread_ = std::thread(std::bind(&AsyncLogging::thread_pro,this));
}

AsyncLogging::~AsyncLogging()
{
    running_ = false;
    thread_.join();
} 
void AsyncLogging::append(const char* log_line,size_t len)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(cur_buffer_->valid() > len)
    {
        cur_buffer_->append(log_line,len);
    }
    else
    {
        buffers_.push_back(std::move(cur_buffer_));
        if(next_buffer_ != nullptr)
        {
            cur_buffer_ = std::move(next_buffer_);
        }
        else
        {
            //内存的新增，说明当前的缓存大小不适应当前的日志规模
            cur_buffer_.reset(new Buffer);
            //TODO：添加一条本地日志，用作提示
        }
        cur_buffer_->append(log_line,len);
        condition_.notify_one();
    }
} 
//定时清理或者数据写满清理一次
int AsyncLogging::thread_pro(void)
{
    Bufferptr temp_buffer1 (new Buffer());
    Bufferptr temp_buffer2 (new Buffer());
    Buffers buffers_to_write;
    temp_buffer1->clear();
    temp_buffer2->clear();
    buffers_to_write.reserve(16); 
    auto sec = std::chrono::seconds(1);
    LogFile log_file(log_file_path_,roll_size_);
    while(running_)
    {
        assert(temp_buffer1 != nullptr);
        assert(temp_buffer2 != nullptr);
        assert(buffers_to_write.empty());
        {
          std::unique_lock<std::mutex> lock(mutex_);
          condition_.wait_for(lock,sec*3, [this]{return !buffers_.empty();});
          //copy user data;
          buffers_.push_back(std::move(cur_buffer_));
          //refresh cur_buffer
          cur_buffer_ = std::move(temp_buffer1);
          buffers_to_write.swap(buffers_);
          if(next_buffer_ == nullptr)
          {
              next_buffer_ = std::move(temp_buffer2);
          }
          //现在cur_/next_/buffers_都是全新的,可以解锁
        }
        //下面要做的事情是将buffers_to_write中的数据写进去,并恢复temp_buffer1.2
        assert(!buffers_to_write.empty());
        for(auto& it : buffers_to_write)
        {
            if(it->len() > 0)
            {
                log_file.append(it->data(), it->len());
                //record += it->len();
                it->clear();
            }
        }
        if(temp_buffer1 == nullptr)
        {
            assert( buffers_to_write.size() >= 1);
            temp_buffer1 = std::move(buffers_to_write.back());
            temp_buffer1->clear();
            buffers_to_write.pop_back();
        }
        if(temp_buffer2 == nullptr)
        {
            assert( buffers_to_write.size() >= 1);
            temp_buffer2 = std::move(buffers_to_write.back());
            temp_buffer2->clear();
            buffers_to_write.pop_back();
        }
        //析构,会释放新增的内存
        log_file.flush();
        buffers_to_write.clear();
    }    
    log_file.flush();
    return 0;
}

}
}
}
