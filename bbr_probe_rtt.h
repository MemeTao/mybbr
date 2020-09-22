#ifndef BBR_PORBE_RTT_H_
#define BBR_PROBE_RTT_H_

#include <cstddef>
#include <cstdint>
#include <vector>
#include <bbr_mode.h>
#include <bandwidth_sampler.h>
#include <time/timestamp.h>

namespace bbr
{

class BbrAlgorithm;
class BbrModel;
class BbrCongestionEvent;

class BbrProbeRtt
{
public:
    BbrProbeRtt(BbrAlgorithm* bbr,  BbrModel* model);

    void enter(time::Timestamp now,
               const BbrCongestionEvent* congestion_event);

    BbrMode OnCongestionEvent(
        size_t prior_inflight,
        time::Timestamp at_time,
        const std::vector<AckedPacket>& acked_packets,
        const std::vector<LostPacket>& lost_packets,
        const BbrCongestionEvent& congestion_event);

    size_t inflight_target() const;

private:
    BbrAlgorithm* bbr_;
    BbrModel* model_;

    time::Timestamp exit_time_;
};
}
#endif
