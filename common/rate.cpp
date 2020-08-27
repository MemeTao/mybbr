#include <common/rate.h>
#include <limits>

namespace bbr
{
namespace common
{
std::string BitRate::to_str()const
{
    if(bit_rate_ == std::numeric_limits<int64_t>::max()){
        return "+inf bps";
    }
    if(bit_rate_ == std::numeric_limits<int64_t>::min()){
        return "-inf bps";
    }

    char buf[64] = {0};
    int64_t divisor = 1;
    std::string unit;

    if(bit_rate_ < 10000){
        unit = "bps";
    }else if(bit_rate_ < 1000 * 1000){
        divisor = 1000;
        unit = "kbps";
    }else{
        divisor = 1000 * 1000;
        unit = "mbps";
    }
    if(divisor > 1  && bit_rate_ % divisor)
        snprintf(buf, sizeof buf,"%.3f", bit_rate_ * 1.0 / divisor);
    else
        snprintf(buf, sizeof buf,"%ld", bit_rate_  / divisor);

    return std::string(buf) + unit;
}

BitRate BitRate::positive_infinity()
{
    return BitRate(std::numeric_limits<int64_t>::max());
}

BitRate BitRate::negative_infinity()
{
    return BitRate(std::numeric_limits<int64_t>::min());
}

namespace rate{

BitRate operator"" _bps(unsigned long long h)
{
    return BitRate(static_cast<int64_t>(h));
}

BitRate operator"" _kbps(unsigned long long k)
{
    return 1024_bps * k;
}

BitRate operator"" _mbps(unsigned long long m)
{
    return 1024_kbps * m;
}

}
}
}
