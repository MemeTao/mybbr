#ifndef COMMON_RANDOM_H_
#define COMMON_RANDOM_H_

#include <stdint.h>
#include <limits>
#include <cassert>

namespace rtc{
namespace common{
//FIXME:修改为C++11的标准算法
class Random{
public:
    explicit Random(uint64_t seed);
    template <typename T>
    T rand() {
        static_assert(std::numeric_limits<T>::is_integer &&
                      std::numeric_limits<T>::radix == 2 &&
                      std::numeric_limits<T>::digits <= 32,
                        "Rand is only supported for built-in integer types that are "
                        "32 bits or smaller.");
        return static_cast<T>(next_output());
    }

    // Uniformly distributed pseudo-random number in the interval [0, t].
    uint32_t rand(uint32_t t);

    // Uniformly distributed pseudo-random number in the interval [low, high].
    uint32_t rand(uint32_t low, uint32_t high);

    // Uniformly distributed pseudo-random number in the interval [low, high].
    int32_t rand(int32_t low, int32_t high);

    // Normal Distribution.
    double gaussian(double mean, double standard_deviation);

    // Exponential Distribution.
    double exponential(double lambda);
private:
    // Outputs a nonzero 64-bit random number.
    uint64_t next_output() 
    {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        assert(state_ != 0x0ULL);
        return state_ * 2685821657736338717ull;
    }
    uint64_t state_;
};

// Return pseudo-random number in the interval [0.0, 1.0).
/*template <>
float Random::rand<float>();

// Return pseudo-random number in the interval [0.0, 1.0).
template <>
double Random::rand<double>();

// Return pseudo-random boolean value.
template <>
bool Random::rand<bool>();*/

}
}
#endif
