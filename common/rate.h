#ifndef BBR_COMMON_RATE_H_
#define BBR_COMMON_RATE_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <math.h>
#include <type_traits>
#include <string>

namespace bbr
{
namespace common
{

class BitRate
{
public:
    explicit BitRate(int64_t rate = positive_infinity().value())
        :bit_rate_(rate)
    {}
    int64_t value() const { return bit_rate_;}
    bool is_valid() const{ 
        return value() != positive_infinity().value()
            && value() != negative_infinity().value();
    }

    static BitRate positive_infinity();
    static BitRate negative_infinity();

    std::string to_str()const;
private:
    int64_t bit_rate_; //bits per second
};

using BandWidth = BitRate;

inline bool operator == (const BitRate d1,const BitRate d2)
{
    return d1.value() == d2.value();
}

inline bool operator != (const BitRate d1,const BitRate d2)
{
    return !(d1 == d2);
}

inline bool operator < (const BitRate d1,const BitRate d2)
{
    return d1.value() < d2.value();
}

inline bool operator <= (const BitRate d1,const BitRate d2)
{
    return d1.value() <= d2.value();
}

inline bool operator > (const BitRate d1,const BitRate d2)
{
    return d1.value() > d2.value();
}

inline bool operator >= (const BitRate d1,const BitRate d2)
{
    return d1.value() >= d2.value();
}

template<typename T>
inline BitRate operator + (const BitRate d1,const T d2)
{
    //不允许使用 1_bps + 1 = 2_bps等乱七八糟的操作
    //正确用法师 1_bps + 1_bps = 2_bps
    static_assert(!std::is_integral<T>::value,"1_bps + 1 is not allowed"); 
    return DataRate(d1.value() + d2.value());
}

template<typename T>
inline BitRate operator - (const BitRate d1,const T d2)
{
    static_assert(!std::is_integral<T>::value,"1_ms - 1 is not allowed"); 
    return BitRate(d1.value() - d2.value());
}

template<typename T>
inline BitRate operator * (const BitRate d1,const T d2)
{
    //不允许 3_bps * 2_bps 等乱七八糟的操作
    static_assert(std::is_integral<T>::value |
            std::is_floating_point<T>::value,"integral or float is required");
    return BitRate(static_cast<int64_t>(d1.value() * d2));
}

template<typename T>
inline BitRate operator * (const T d2, const BitRate d1)
{
    return d1 * d2;
}

template<typename T>
inline BitRate operator / (const BitRate d1, const T d2)
{
    static_assert(std::is_integral<T>::value |
            std::is_floating_point<T>::value, "integral or float is required"); 
    BitRate ret(BitRate::positive_infinity().value());
    //ret = DataRate(std::lround(d1.value() * 1.0 / d2));
    ret = BitRate(static_cast<int64_t>(d1.value()*1.0/d2 + 0.5));
    return ret;
}
//允许使用: 
//      1_mps / 1_kbps = 1000;
inline double operator / (const BitRate d1, const BitRate d2)
{
    return d1.value() * 1.0 / d2.value();
}

namespace rate
{
//user-defined literals 
BitRate operator"" _bps(unsigned long long b);
BitRate operator"" _kbps(unsigned long long k);
BitRate operator"" _mbps(unsigned long long m);
}
}
}
#endif
