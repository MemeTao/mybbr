#ifndef BBR_COMMON_H_
#define BBR_COMMON_H_

#include <cstddef>
#include <cstdint>
#include <time/timestamp.h>

namespace bbr
{
namespace internal
{
struct AckedPacket
{
    uint64_t seq_no = 0;
    size_t bytes = 0;
    time::Timestamp receive_time; //the packet received by the peer
};

struct LostPacket
{
    uint64_t seq_no = 0;
    size_t bytes = 0;
};
}
}
#endif
