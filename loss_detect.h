#ifndef BBR_LOSS_DETECT_H_
#define BBR_LOSS_DETECT_H_

#include <cstddef>
#include <cstdint>
#include <vector>
#include <bbr.h>
#include <deque>
#include <time/timestamp.h>

namespace bbr
{
template <typename PacketType,
    template<typename Elem,
        typename Allocator = std::allocator<Elem>>
            class ContainerType = std::deque>
class PacketHistory;

class LossDetect
{
    const uint64_t kDefaultThreshold = 2;
public:
    std::vector<uint64_t> on_pkt_ack(AckedPacket pkt);

    std::vector<uint64_t> on_pkts_ack(
            const std::vector<AckedTrunk>& blocks);

public:
    void set_reordering_threshold(uint64_t threshold);
    void set_reordering_timeout(time::TimeDelta timeout);

private:
    PacketHistory<SendingPacket> pkts_history_;

    size_t reordering_threshold_ = kDefaultThreshold;
};
}
#endif
