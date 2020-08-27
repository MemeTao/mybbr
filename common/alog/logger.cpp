#include <rtc/common/alog/logger.h>
#include <stdio.h>
#include <algorithm>
#include <rtc/common/alog/async_logging.h>
#include <rtc/common/alog/logstream.h>

using std::placeholders::_1;
using std::placeholders::_2;

namespace rtc
{
namespace common
{
namespace alog
{
void default_output(const char* data,size_t size)
{
    size_t len = ::fwrite(data,1,size,stdout);
    (void) len;
} 
void default_flush()
{
    fflush(stdout);
}

static Logger::OutputFunc g_output = std::bind(default_output,_1,_2);
static Logger::FlushFunc g_flash = std::bind(default_flush);

static std::shared_ptr<AsyncLogging> asyn_logger = nullptr;

void rtc_log_init(const char* log_file_path,uint64_t rollsize)
{
    std::string real_path(log_file_path);
    if(rollsize < 1024 * 1024)
        rollsize = 1024 * 1024;
    asyn_logger.reset(new AsyncLogging(real_path,rollsize));
    g_output = std::bind(&AsyncLogging::append,asyn_logger.get(),_1,_2);
}
void rtc_log_uinit()
{
    if(asyn_logger != nullptr)
    {
        asyn_logger.reset();
    }
}

Logger::LogLevel Logger::g_network_log_level = Logger::kDebug;

Logger::~Logger()
{
    impl_.endline();
    LogStream::Buffer& buffer(impl_.stream_.buffer()); 
    g_output(buffer.data(),static_cast<int>(buffer.len()));
//    if(impl_.level_ == kError)
//    {
//        g_flash();
//    }
}

std::string cut_slash(const char* path,size_t num)
{
    size_t len = std::strlen(path);
    size_t slash_count = 0;
    size_t index = 0;
    for(size_t i = len ; i > 0 ;i --){
        if(path[i] == '\\' || path[i] == '/'){
            slash_count ++;
            index = i;
            if(slash_count >= num){
                return std::string(path + index, len - index);

            }
        }
    }
    return std::string(path);
}

std::string dump(const uint8_t* data,size_t len, size_t print)
{
    char buffer[1024] = {0};
    size_t print_len = std::min(len,print);
    size_t wrap = 0;
    for(size_t i = 0 ; i < print_len;i++){
        snprintf(buffer + i + wrap,sizeof buffer,"%x ",data[i]);
        if(i % 16 == 0){
            buffer[i+1] = '\n';
            wrap += 1;
        }
    }
    std::string log(buffer);
    log.resize(print_len + wrap);
    return log;
}

}
}
}

