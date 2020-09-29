#ifndef BBR_LOSS_DETECT_H_
#define BBR_LOSS_DETECT_H_

#include <cstddef>
#include <cstdint>
#include <vector>
#include <time/timestamp.h>

namespace bbr
{
class PacketHistory;

class LossDetect
{
    const uint64_t kDefaultThreshold = 2;
public:
    struct AckedPkt {
        uint64_t seq_no = 0;
        time::Timestamp arrival_time;  //peer timestamp
    };
    struct AckTrunk {
        AckedPkt begin = 0;
        AckedPkt end = 0;
    };

    std::vector<uint64_t> on_pkts_ack(AckedPkt left,
            AckedPkt right,
            const std::vector<AckTrunk>& blocks);

public:
    void set_reordering_threshold(uint64_t threshold);
    void set_reordering_timeout(time::TimeDelta timeout);

private:
    PacketHistory* pkts_history_;

    size_t reordering_threshold_ = kDefaultThreshold;
};
}
#endif
