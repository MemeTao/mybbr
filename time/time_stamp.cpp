#include <time/time_stamp.h>
#ifdef __unix__
#include <sys/time.h>  //gettimeofday();
#endif //__unix__
#if defined(_WIN32) || defined(WIN32) 
#include <ctime>
#include <chrono>
#endif  //__win32
#include <cinttypes>

static int64_t microseconds_since_epoch()
{
    int64_t time_cur = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
    return time_cur;
//    static int64_t clock_fre= clock_frequence();
//    static int64_t clocks_start = clock_count();
//    int64_t clocks_since_power_up = clock_count();
//    int64_t micro_seconds = 0;
//    if(clock_fre != 0
//        && clocks_since_power_up != 0
//        && clocks_start != 0)
//    {
//        micro_seconds = time_start + static_cast<int64_t>((clocks_since_power_up - clocks_start) * 1000 * 1000.0 / double(clock_fre*1.0)) ;
//    }
//    else
//    {
//            micro_seconds = std::chrono::duration_cast<std::chrono::microseconds>(
//            std::chrono::system_clock::now().time_since_epoch()).count();
//    }
//    return micro_seconds;
}

namespace bbr
{
namespace time
{

Timestamp Timestamp::now(Type t)
{
    if(t == Type::kSincePowerup){
        return std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    return Timestamp((static_cast<int64_t>(8) * 3600) * kMicroSecondsPerSecond + ::microseconds_since_epoch());
}

Timestamp Timestamp::positive_infinity()
{
    return Timestamp(std::numeric_limits<int64_t>::max());
}

Timestamp Timestamp::negative_infinity()
{
    return Timestamp(std::numeric_limits<int64_t>::min());
}

std::string Timestamp::to_string(bool show_microseconds)const 
{
    char buf[32] = {0};
    time_t seconds = static_cast<time_t>(microseconds_ / kMicroSecondsPerSecond);
    struct tm tm_time;
#if defined(WIN32) || defined(_WIN32)
    //gmtime is thread safe on windows
    tm_time = *(gmtime(&seconds));
#endif

#ifdef __unix__
    gmtime_r(&seconds, &tm_time);
#endif

    if (show_microseconds)
    {
        int microseconds = static_cast<int>(microseconds_ % kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
             microseconds);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    return buf;
}
}
}
