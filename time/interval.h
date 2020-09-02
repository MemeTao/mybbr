#ifndef BBR_TIME_INTERVAL_H_
#define BBR_TIME_INTERVAL_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <limits>
#include <math.h>
#include <string>
#include <common/rate.h>

namespace bbr
{
namespace time
{
class TimeDelta
{
public:
     explicit TimeDelta(int64_t us = std::numeric_limits<std::int64_t>::max())
        :delta_us_(us)
    {}
    int64_t value() const {
        return delta_us_;
    }
    bool is_valid()const 
    { 
        return value() != positive_infinity().value()
            && value() != negative_infinity().value();
    }
    std::string to_log()const;
    static TimeDelta positive_infinity()
    {
        return TimeDelta(std::numeric_limits<std::int64_t>::max());
    }
    static TimeDelta negative_infinity()
    {
        return TimeDelta(std::numeric_limits<std::int64_t>::min());
    }

private:
    int64_t delta_us_;
};

inline bool operator < (const TimeDelta& t1,const TimeDelta& t2)
{
    return t1.value() < t2.value();
}

inline bool operator <= (const TimeDelta& t1,const TimeDelta& t2)
{
    return t1.value() <= t2.value();
}

inline bool operator > (const TimeDelta& t1,const TimeDelta& t2)
{
    return t1.value() > t2.value();
}

inline bool operator >= (const TimeDelta& t1,const TimeDelta& t2)
{
    return t1.value() >= t2.value();
}

inline bool operator == (const TimeDelta& t1,const TimeDelta& t2)
{
    return t1.value() == t2.value();
}

inline bool operator != (const TimeDelta& t1,const TimeDelta& t2)
{
    return !(t1.value() == t2.value());
}

/**
 *加减乘除
 */
template<typename T>
inline TimeDelta operator + (const TimeDelta d1,const T d2)
{
    //不允许使用 1_ms + 1 这样的操作
    //正确使用格式为: 1_ms + 1_ms （两边都是TimeDelta类型)
    static_assert(!std::is_integral<T>::value,"1_ms + 1 is not allowed"); 
    return TimeDelta(d1.value() + d2.value());
}

template<typename T>
inline TimeDelta operator - (const TimeDelta d1,const T d2)
{
    static_assert(!std::is_integral<T>::value,"1_ms - 1 is not allowed"); 
    return TimeDelta(d1.value() - d2.value());
}

template<typename T>
inline TimeDelta operator * (const TimeDelta d1,const T d2)
{
    //不允许 3ms * 2us 等乱七八糟的操作
    //只可以 3ms * 2 = 6ms
    static_assert(std::is_integral<T>::value ||
            std::is_floating_point<T>::value, "integral or float is required"); 
    //return TimeDelta(std::lround(d1.value() * d2 * 1.0));
    return TimeDelta(static_cast<int64_t>(d1.value()*d2*1.0 + 0.5));
}

template<typename T>
inline TimeDelta operator / (const TimeDelta d1,const T d2)
{
    static_assert(std::is_integral<T>::value 
            || std::is_floating_point<T>::value, "integral or float is required"); 
    TimeDelta ret(TimeDelta::positive_infinity().value());
    //XXX:是否该替使用者考虑除数为0的问题
    //除数为0，就让它奔溃好了
    //ret = TimeDelta(std::lround(d1.value() * 1.0 / d2 ));
    ret = TimeDelta(static_cast<int64_t>(d1.value()*1.0/d2 + 0.5));
    return ret;
}
//允许使用: 
//      1ms / 1us = 1000;
//      1ms / 3ms = 0.33333
inline double operator / (const TimeDelta d1, const TimeDelta d2)
{
    return d1.value() * 1.0 / d2.value();
}

inline common::BitRate operator / (const size_t bytes, const time::TimeDelta dt)
{
    using namespace time;
    return common::BitRate(static_cast<int64_t>(bytes * 8 / (dt / time::TimeDelta(1*1000*1000))));
}

inline time::TimeDelta operator / (const size_t bytes, const common::BitRate bps)
{
    using namespace time;
    return time::TimeDelta(1*1000*1000) * (bytes * 8.0f / bps.value());
}

//速度 * 时间间隔 = 比特数
inline size_t operator * (const common::BitRate d1, const time::TimeDelta dt)
{
    using namespace time;
    //return static_cast<size_t>(std::lround(d1.value() * (dt / 1_sec)));
    return static_cast<size_t>(d1.value() * (dt/time::TimeDelta(1*1000*1000)) + 0.5);
}

//user-defined literals 
TimeDelta operator"" _hour(unsigned long long h);
TimeDelta operator"" _min(unsigned long long m);
TimeDelta operator"" _sec(unsigned long long s);
TimeDelta operator"" _ms(unsigned long long ms);
TimeDelta operator"" _us(unsigned long long us);

}
}
#endif
