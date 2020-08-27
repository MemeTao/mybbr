#ifndef ASYNC_LOGGING_H_
#define ASYNC_LOGGING_H_

#include <rtc/common/alog/logstream.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
namespace rtc{
namespace common{
namespace alog{

class AsyncLogging{
    using Buffer    = FixedBuffer<LogStream::kLargeBuffer>;
    using Bufferptr = std::unique_ptr<Buffer>;
    using Buffers   = std::vector<std::unique_ptr<Buffer>>;
public:
    AsyncLogging(const std::string& file_path,uint64_t roll_size=0);
    ~AsyncLogging();
    void append(const char* content,size_t size);
private:
    int thread_pro();

    std::string             log_file_path_;
    uint64_t                roll_size_; 
    uint32_t                reserve_days_;
    Bufferptr               cur_buffer_;
    Bufferptr               next_buffer_;
    Buffers                 buffers_;

    bool                    running_;
    std::mutex              mutex_;
    std::condition_variable condition_;
    std::thread             thread_;
};

}
}
}
#endif
