#include <bbr_drain.h>
#include <bbr_model.h>
#include <bbr_algorithm.h>

namespace bbr
{
BbrDrainMode::BbrDrainMode(BbrAlgorithm* bbr,  BbrModel* model)
    :bbr_(bbr),
     model_(model)
{
    ;
}

BbrMode BbrDrainMode::OnCongestionEvent(
        size_t prior_inflight,
        time::Timestamp at_time,
        const std::vector<AckedPacket>& acked_packets,
        const std::vector<LostPacket>& lost_packets,
        const BbrCongestionEvent& congestion_event)
{
    model_->set_pacing_gain(bbr_->params().drain_pacing_gain);
    model_->set_cwnd_gain(bbr_->params().drain_cwnd_gain);

    size_t drain_target = drain_target();
    if (congestion_event.bytes_in_flight <= drain_target) {
        //TODO: log something
        return BbrMode::PROBE_BW;
    }

    return BbrMode::DRAIN;
}

size_t BbrDrainMode::cwnd_limit() const
{
    return model_->inflight_lo();
}

size_t BbrDrainMode::drain_target() const
{
    size_t bdp = model_->bdp(model_->max_bw());
    return std::max(bdp, bbr_->min_cwnd());
}
}
