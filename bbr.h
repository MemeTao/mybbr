#ifndef BBR_BBR_H_
#define BBR_BBR_H_

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <time/timestamp.h>

namespace bbr
{

struct SendingPacket {
    enum Interface : uint8_t {
        kRowPointer = 1,
        kVector,
        kSharedPtr
    };
    uint64_t seq_no = 0;

    Interface type = Interface::kSharedPtr;

    const uint8_t* raw_pkt;

    std::vector<uint8_t> vec_pkt;

    std::shared_ptr<uint8_t> shared_ptr_pkt;

    size_t pkt_size = 0;
};

struct AckedPkt {
    uint64_t seq_no = 0;
    time::Timestamp arrival_time;  //time of this packet received by peer
};
}
#endif
