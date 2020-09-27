#ifndef BBR_DRAIN_H_
#define BBR_DRAIN_H_

#include <vector>
#include <common/rate.h>
#include <time/timestamp.h>
#include <bbr_common.h>
#include <bbr_mode.h>

namespace bbr
{
class BbrAlgorithm;
class BbrModel;
struct BbrCongestionEvent;
class BbrDrainMode
{
public:
    BbrDrainMode(BbrAlgorithm* bbr,  BbrModel* model);

    bool is_probing() const {
        return false;
    }
    size_t cwnd_limit() const;

    BbrMode OnCongestionEvent(
        size_t prior_inflight,
        time::Timestamp at_time,
        const std::vector<AckedPacket>& acked_packets,
        const std::vector<LostPacket>& lost_packets,
        const BbrCongestionEvent& congestion_event);

private:
    size_t drain_target() const;

private:
    BbrAlgorithm* bbr_;
    BbrModel* model_;
};

}
#endif
