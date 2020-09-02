#include <time/time_interval.h>
#include <time/time_stamp.h>

namespace bbr
{
namespace time
{ 
std::string TimeDelta::to_log()const
{
    std::string log;
    if(delta_us_ == std::numeric_limits<int64_t>::max()){
        log = "+inf";
        return log;
    }
    if(delta_us_ == std::numeric_limits<int64_t>::min()){
        log = "-inf";
        return log;
    }
    char buf[64] = {0};
    std::string unit;
    int64_t divisor = 1;
    if(delta_us_ < 1000){
        unit = "us";
    }else{
        unit = "ms";
        divisor = 1000;
    }
    if(divisor > 1 && delta_us_ % divisor ){
        snprintf(buf,sizeof buf,"%.3f", delta_us_ * 1.0 / divisor);
    }else{
        snprintf(buf,sizeof buf, "%ld", delta_us_ / divisor);
    }

    return std::string(buf) + unit;
}

//TODO: day and month?
TimeDelta operator"" _hour(unsigned long long h)
{
    return TimeDelta(static_cast<int64_t>(h) * 60 * 60
            * Timestamp::kMicroSecondsPerSecond);
}
TimeDelta operator"" _min(unsigned long long m)
{
    return TimeDelta(static_cast<int64_t>(m) *  60
            * Timestamp::kMicroSecondsPerSecond);
}
TimeDelta operator"" _sec(unsigned long long s)
{
    return TimeDelta(static_cast<int64_t>(s)  
            * Timestamp::kMicroSecondsPerSecond);
}
TimeDelta operator"" _ms(unsigned long long ms)
{
    return TimeDelta(static_cast<int64_t>(ms)  
            * 1000);
}
TimeDelta operator"" _us(unsigned long long us)
{ 
    return TimeDelta( static_cast<int64_t>(us)  
            * 1);
}

}
}
