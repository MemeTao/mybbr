#ifndef COMMON_ALOG_LOGGER_H_
#define COMMON_ALOG_LOGGER_H_
#include <memory>
#include <functional>
#include <thread>
#include <sstream>
#include <rtc/common/alog/logstream.h>
#include <rtc/common/time_stamp.h>

namespace rtc
{
namespace common
{
namespace alog
{

void rtc_log_init(const char* log_file_path = "./", uint64_t rollsize=1024*1024);
void rtc_log_uinit();
std::string cut_slash(const char* path,size_t num);
std::string dump(const uint8_t* data,size_t len, size_t print);
class Logger
{
public:
    enum LogLevel{
        kDebug = 1,
        kInfo  = 2,
        kWarning  = 4,
        kError = 255,
    };
    Logger(const LogLevel level)
        :impl_(level){}
    ~Logger();
    
    using OutputFunc = std::function<void(const char*,int)>;
    using FlushFunc = std::function<void(void)>;
public:
    static LogLevel g_network_log_level;
    static LogLevel loger_level() { return g_network_log_level;} 
    static void set_log_level(LogLevel level){g_network_log_level = level;}
    static void set_output_func(OutputFunc& func);
    static void set_flash_func(FlushFunc& func);

    LogStream& stream() { return impl_.stream_;}
private:
    class Impl{
    public:
        Impl(LogLevel level) : level_(level){}
        void endline() { stream_<<"\n";}
        LogStream stream_;
        LogLevel level_;
    };
    Impl impl_;
};
inline std::string thread_id()
{
    std::ostringstream os;
    os << std::this_thread::get_id();
    return std::string("[") + os.str() + "]";
}
}
}
}

#define rtc_debug if (rtc::common::alog::Logger::loger_level() <= rtc::common::alog::Logger::kDebug) \
        rtc::common::alog::Logger(rtc::common::alog::Logger::kDebug).stream() \
        <<"[debug]"<< rtc::common::alog::thread_id()<<" "<<\
        rtc::common::Timestamp::now().to_string()<<" "<<rtc::common::alog::cut_slash(__FILE__,2)<<"::"<<__LINE__<<" "
#define rtc_info if (rtc::common::alog::Logger::loger_level() <= rtc::common::alog::Logger::kInfo) \
        rtc::common::alog::Logger(rtc::common::alog::Logger::kInfo).stream()<<\
        "[info]" <<rtc::common::alog::thread_id()<<" "<< \
        rtc::common::Timestamp::now().to_string()<<" "<<rtc::common::alog::cut_slash(__FILE__,2)<<"::"<<__LINE__<<" "
#define rtc_warning if (rtc::common::alog::Logger::loger_level() <= rtc::common::alog::Logger::kWarning) \
        rtc::common::alog::Logger(rtc::common::alog::Logger::kWarning).stream()<< \
        "[warning]"<<rtc::common::alog::thread_id()<<" "<< \
        rtc::common::Timestamp::now().to_string()<<" "<<rtc::common::alog::cut_slash(__FILE__,2)<<"::"<<__LINE__<<" "
#define rtc_error if (rtc::common::alog::Logger::loger_level() <= rtc::common::alog::Logger::kError) \
        rtc::common::alog::Logger(rtc::common::alog::Logger::kError).stream()<<\
        "[error]" <<rtc::common::alog::thread_id()<<" "<< \
        rtc::common::Timestamp::now().to_string()<<" "<<rtc::common::alog::cut_slash(__FILE__,2)<<"::"<<__LINE__<<" "
#endif
