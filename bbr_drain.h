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
    size_t cwnd_upper_limit() const;

    BbrMode on_congestion_event(
        size_t prior_inflight,
        time::Timestamp at_time,
        const std::vector<AckedPacket>& acked_packets,
        const std::vector<LostPacket>& lost_packets,
        const BbrCongestionEvent& congestion_event);

    void enter(time::Timestamp now,
               const BbrCongestionEvent* congestion_event) {};

    void leave(time::Timestamp now,
               const BbrCongestionEvent* congestion_event) {};


    BbrMode on_exit_quiescence(time::Timestamp,
            time::Timestamp ) { return BbrMode::DRAIN;}

private:
    size_t drain_target() const;

private:
    BbrAlgorithm* bbr_;
    BbrModel* model_;
};

}
#endif
