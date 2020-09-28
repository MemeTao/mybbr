#include <bbr_probe_rtt.h>
#include <bbr_model.h>
#include <bbr_algorithm.h>

namespace bbr
{
BbrProbeRtt::BbrProbeRtt(BbrAlgorithm* bbr,  BbrModel* model)
    :bbr_(bbr),
     model_(model)
{
    ;
}

void BbrProbeRtt::enter(time::Timestamp now,
        const BbrCongestionEvent* congestion_event)
{
    model_->set_pacing_gain(1.0);
    model_->set_cwnd_gain(1.0);
    exit_time_ = time::Timestamp::positive_infinity();
}


BbrMode BbrProbeRtt::on_congestion_event(
    size_t,
    time::Timestamp ,
    const std::vector<AckedPacket>&,
    const std::vector<LostPacket>&,
    const BbrCongestionEvent& congestion_event)
{
    if(!exit_time_.is_valid()) {
        if(congestion_event.bytes_in_flight <= inflight_target() ||
                congestion_event.bytes_in_flight <=  bbr_->min_cwnd()) {

            exit_time_ = congestion_event.event_time + bbr_->params().probe_rtt_duration;
        }
        return BbrMode::PROBE_RTT;
    }
    return congestion_event.event_time > exit_time_ ?
            BbrMode::PROBE_BW : BbrMode::PROBE_RTT;
}

BbrMode BbrProbeRtt::on_exit_quiescence(
        time::Timestamp /*quiescence_start_time*/,
        time::Timestamp now)
{
    if (now > exit_time_) {
        return BbrMode::PROBE_BW;
    }
    return BbrMode::PROBE_RTT;
}

size_t BbrProbeRtt::inflight_target() const
{
    return model_->bdp(model_->max_bw(),
            bbr_->params().probe_rtt_inflight_target_bdp_fraction);
}

size_t BbrProbeRtt::cwnd_upper_limit() const
{
    size_t inflight_upper_bound =
        std::min(model_->inflight_lo(), model_->inflight_hi_with_headroom());
    return std::min(inflight_upper_bound, inflight_target());
}
}

