#ifndef BBR_COMMON_SEQUENCE_WRPPER_H_
#define BBR_COMMON_SEQUENCE_WRPPER_H_

#include <limits>
#include <cstdint>

namespace rtc
{
namespace common
{

template <typename U>
class Unwrapper {
    static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned");
    static_assert(std::numeric_limits<U>::max() <=
                    std::numeric_limits<uint32_t>::max(),
                    "U must not be wider than 32 bits");
public:
    inline bool is_newer(U value, U prev_value)const
    {
        static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned");
        constexpr U kBreakpoint = (std::numeric_limits<U>::max() >> 1) + 1;
        if (value - prev_value == kBreakpoint) {
            return value > prev_value;
        }
        return value != prev_value &&
            static_cast<U>(value - prev_value) < kBreakpoint;
    }
public:
    int64_t unwrap_without_update(U value) const 
    {
        if (last_value_ == -1)
        {
            return value;
        }
        constexpr int64_t kMaxPlusOne = static_cast<int64_t>(std::numeric_limits<U>::max()) + 1;

        U cropped_last = static_cast<U>(last_value_);
        int64_t delta = value - cropped_last;
        if (is_newer(value, cropped_last)) {
            if (delta < 0)
                delta += kMaxPlusOne;  // Wrap forwards.
        } else if (delta > 0 && (last_value_ + delta - kMaxPlusOne) >= 0) {
            delta -= kMaxPlusOne;
        }

        return last_value_ + delta;
    }

    void update_last(int64_t last_value) { last_value_ = last_value; }

    int64_t unwrap(U value)
    {
        int64_t unwrapped = unwrap_without_update(value);
        update_last(unwrapped);
        return unwrapped;
    }

private:
    int64_t last_value_ = -1;
};
}
}
#endif
