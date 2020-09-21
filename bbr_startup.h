#ifndef BBR_STARTUP_H_
#define BBR_STARTUP_H_

#include <vector>
#include <common/rate.h>
#include <time/time_stamp.h>
#include <bbr_mode.h>

namespace bbr
{
class BbrAlgorithm;
class BbrModel;
struct AckedPacket;
struct LostPacket;
struct BbrCongestionEvent;

//we won't enter start-up mode more than once
class BbrStartupMode
{
public:
    BbrStartupMode(BbrAlgorithm* bbr, BbrModel* model);

    bool is_probing() const {
        return true;
    }

    BbrMode OnCongestionEvent(
        size_t prior_inflight,
        time::Timestamp at_time,
        const std::vector<AckedPacket>& acked_packets,
        const std::vector<LostPacket>& lost_packets,
        const BbrCongestionEvent& congestion_event);

private:
    void check_full_bw_reached(const BbrCongestionEvent& congestion_event);
    void Check_excessive_losses(const BbrCongestionEvent& congestion_event);

private:
    BbrAlgorithm* bbr_;
    BbrModel* model_;

    bool full_bw_reached_ = false;
    common::BandWidth full_bw_baseline_;
    uint64_t rounds_without_bw_growth_ = 0;
};
}
#endif
