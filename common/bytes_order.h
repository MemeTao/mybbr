#ifndef COMMON_BYTES_ORDER_h_
#define COMMON_BYTES_ORDER_h_

#include <cstdint>
namespace rtc{

class BytesOrder{
public:
    static inline uint32_t b_to_l(const uint32_t);
    static inline int32_t b_to_l(const int32_t v)
    {return static_cast<int32_t>(b_to_l(static_cast<uint32_t>(v)));}
    static inline uint16_t b_to_l(const uint16_t);
    static inline int16_t b_to_l(const int16_t v)
    {return static_cast<int16_t>(b_to_l(static_cast<uint16_t>(v)));}
    static inline uint64_t b_to_l(const uint64_t);
    static inline int64_t b_to_l(const int64_t v)
    {return static_cast<int64_t>(b_to_l(static_cast<uint64_t>(v)));}

    static inline uint16_t l_to_b(const uint16_t);
    static inline int16_t l_to_b(const int16_t v)
    {return static_cast<int16_t>(l_to_b(static_cast<uint16_t>(v)));}
    static inline uint32_t l_to_b(const uint32_t);
    static inline int32_t l_to_b(const int32_t v)
    {return static_cast<int32_t>(l_to_b(static_cast<uint32_t>(v)));}
    static inline uint64_t l_to_b(const uint64_t);
    static inline int64_t l_to_b(const int64_t v)
    {return static_cast<int64_t>(l_to_b(static_cast<uint64_t>(v)));}
};

inline uint16_t BytesOrder::b_to_l(const uint16_t value)
{
    return (static_cast<uint16_t>((value & 0x00FF) << 8)) | ((value & 0xFF00) >> 8);
}

inline uint32_t BytesOrder::b_to_l(const uint32_t value)
{
    return ((value & 0x000000FF) << 24) |  ((value & 0x0000FF00) << 8) 
           | ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24);
}

inline uint64_t BytesOrder::b_to_l(const uint64_t value)
{
    uint32_t v_l = static_cast<uint32_t>((value & 0x00000000ffffffff));
    uint32_t v_h = (value & 0xffffffff00000000) >> 32;
    uint32_t temp = v_h;
    v_h = b_to_l(v_l);
    v_l = b_to_l(temp);
    return (static_cast<uint64_t>(v_h) << 32) | v_l;
}

inline uint16_t BytesOrder::l_to_b(const uint16_t value)
{
    return b_to_l(value);
}

inline uint32_t BytesOrder::l_to_b(const uint32_t value)
{
    return b_to_l(value);
}

inline uint64_t BytesOrder::l_to_b(const uint64_t value)
{
    return b_to_l(value);
}

}
#endif
