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
        kRowPointer = 1,  //if can not send immediately, 'memcpy' will be called.
        kVector,          //...., 'vector.swap' will be called.
        kSharedPtr        //...., 'std::move' will be called.
    };
    uint64_t seq_no = 0;

    Interface type = Interface::kSharedPtr;

    const uint8_t* raw_pkt;

    std::vector<uint8_t> vec_pkt;

    std::shared_ptr<uint8_t> shared_ptr_pkt;

    size_t size = 0;
};

struct AckedPacket {
    uint64_t seq_no = 0;
    time::Timestamp arrival_time;  //time of this packet received by peer
};

struct AckedTrunk {
    uint64_t seq_no_begin = 0;
    uint64_t seq_no_end = 0;
    std::vector<time::Timestamp> arrival_times;
};

}
#endif
