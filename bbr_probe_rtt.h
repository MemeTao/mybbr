#ifndef BBR_PORBE_RTT_H_
#define BBR_PROBE_RTT_H_

#include <cstddef>
#include <cstdint>
#include <vector>
#include <bbr_mode.h>
#include <bbr_common.h>
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

    bool is_probing() const { return false;}

    void enter(time::Timestamp now,
               const BbrCongestionEvent* congestion_event);

    void leave(time::Timestamp now,
               const BbrCongestionEvent* congestion_event) {};

    BbrMode on_congestion_event(
        size_t prior_inflight,
        time::Timestamp at_time,
        const std::vector<AckedPacket>& acked_packets,
        const std::vector<LostPacket>& lost_packets,
        const BbrCongestionEvent& congestion_event);

    BbrMode on_exit_quiescence(time::Timestamp quiescence_start_time,
            time::Timestamp now);

    size_t inflight_target() const;

    size_t cwnd_upper_limit() const;

private:
    BbrAlgorithm* bbr_;
    BbrModel* model_;

    time::Timestamp exit_time_;
};
}
#endif
