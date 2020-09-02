#ifndef COMMON_TIME_STAMP_H_
#define COMMON_TIME_STAMP_H_
#include <string>
#include <chrono>

#include "interval.h"

namespace bbr
{
namespace time
{
class Timestamp
{
public:
    enum Type
    {
        kSinceEpoch = 1,
        kSincePowerup
    };
    static const int kMicroSecondsPerSecond = 1000 * 1000;
    static const int64_t kMicroSecondsPerDay = static_cast<int64_t>(kMicroSecondsPerSecond) * 24 * 60 * 60;

    Timestamp()
        :microseconds_(positive_infinity().microseconds())
    {}
    Timestamp(int64_t time_)
        :microseconds_(time_)
    {}
    inline bool is_valid() const{
        return this->microseconds() != negative_infinity().microseconds() &&
            this->microseconds() != positive_infinity().microseconds();
    }

    inline int64_t microseconds() const
    {
        return microseconds_;
    }

    Timestamp& operator += (const TimeDelta dt)
    {
        microseconds_ += dt.value();
        return *this;
    }

    std::string to_string(bool show_microseconds = true) const;

    static Timestamp now(Type since_power_up = kSincePowerup);
    static Timestamp positive_infinity();
    static Timestamp negative_infinity();

private:
    int64_t microseconds_;
};

inline bool operator < (const Timestamp& a,const Timestamp& b)
{
    return a.microseconds()< b.microseconds();
}

inline bool operator <= (const Timestamp& a,const Timestamp& b)
{
    return a.microseconds()<= b.microseconds();
}

inline bool operator > (const Timestamp& a,const Timestamp& b)
{
    return b < a ;
}

inline bool operator >= (const Timestamp& a,const Timestamp& b)
{
    return b <= a ;
}

inline bool operator==(const Timestamp& lhs, const Timestamp& rhs)
{
    return lhs.microseconds() == rhs.microseconds();
}
inline bool operator !=(const Timestamp& lhs, const Timestamp& rhs)
{
    return !(lhs.microseconds() == rhs.microseconds());
}

inline TimeDelta operator-(const Timestamp& lhs,const Timestamp& rhs)
{
    return TimeDelta(lhs.microseconds() - rhs.microseconds());
}

inline Timestamp operator-(const Timestamp& lhs,const TimeDelta& dt)
{
    return Timestamp(lhs.microseconds() - dt.value());
}

inline int64_t operator + (const Timestamp& at_time,const TimeDelta dt)
{
    return at_time.microseconds() + dt.value();
}

}
}
#endif
