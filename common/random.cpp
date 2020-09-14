#include <common/random.h>
#include <math.h>

namespace bbr
{
namespace common
{
Random::Random(uint64_t seed)
{
    //assert(seed != 0x0ull);
    state_ = seed;
}

uint32_t Random::rand(uint32_t t)
{
    // Casting the output to 32 bits will give an almost uniform number.
    // Pr[x=0] = (2^32-1) / (2^64-1)
    // Pr[x=k] = 2^32 / (2^64-1) for k!=0
    // Uniform would be Pr[x=k] = 2^32 / 2^64 for all 32-bit integers k.
    uint32_t x = static_cast<uint32_t>(next_output());
    // If x / 2^32 is uniform on [0,1), then x / 2^32 * (t+1) is uniform on
    // the interval [0,t+1), so the integer part is uniform on [0,t].
    uint64_t result = x * (static_cast<uint64_t>(t) + 1);
    result >>= 32;
    return static_cast<uint32_t>(result);
}

uint32_t Random::rand(uint32_t low, uint32_t high)
{
    return rand(high - low) + low;
}

int32_t Random::rand(int32_t low, int32_t high)
{
    const int64_t low_i64{low};
    return static_cast<int32_t>(rand(static_cast<uint32_t>(high - low_i64)) + low_i64);
}

template <>
float Random::rand<float>() {
    double result = static_cast<double>(next_output() - 1);
    result = result / 0xFFFFFFFFFFFFFFFEull;
    return static_cast<float>(result);
}

template <>
double Random::rand<double>()
{
    double result = static_cast<double>(next_output() - 1);
    result = result / 0xFFFFFFFFFFFFFFFEull;
    return result;
}

template <>
bool Random::rand<bool>()
{
    return rand(0, 1) == 1;
}

double Random::gaussian(double mean, double standard_deviation)
{
    // Creating a Normal distribution variable from two independent uniform
    // variables based on the Box-Muller transform, which is defined on the
    // interval (0, 1]. Note that we rely on NextOutput to generate integers
    // in the range [1, 2^64-1]. Normally this behavior is a bit frustrating,
    // but here it is exactly what we need.
    const double kPi = 3.14159265358979323846;
    double u1 = static_cast<double>(next_output()) / 0xFFFFFFFFFFFFFFFFull;
    double u2 = static_cast<double>(next_output()) / 0xFFFFFFFFFFFFFFFFull;
    return mean + standard_deviation * sqrt(-2 * log(u1)) * cos(2 * kPi * u2);
}

double Random::exponential(double lambda)
{
    double uniform = rand<double>();
    return -log(uniform) / lambda;
}

}
}
