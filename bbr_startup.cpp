#include <bbr_startup.h>
#include <common/rate.h>
#include <bbr_algorithm.h>
#include <bbr_model.h>

namespace bbr
{
BbrStartupMode::BbrStartupMode(BbrAlgorithm* bbr, BbrModel* model)
    :bbr_(bbr),
    model_(model)
{

}
BbrMode BbrStartupMode::on_congestion_event(
    size_t,
    time::Timestamp,
    const std::vector<AckedPacket>&,
    const std::vector<LostPacket>&,
    const BbrCongestionEvent& congestion_event)
{
    check_full_bw_reached(congestion_event);

    Check_excessive_losses(congestion_event);

    model_->set_cwnd_gain(bbr_->params().startup_cwnd_gain);
    model_->set_pacing_gain(bbr_->params().startup_pacing_gain);

    // TODO: Maybe implement STARTUP => PROBE_RTT.
    return full_bw_reached_ ? BbrMode::DRAIN : BbrMode::STARTUP;
}

void BbrStartupMode::check_full_bw_reached(
        const BbrCongestionEvent& congestion_event)
{
    if (full_bw_reached_ || !congestion_event.end_of_round_trip ||
        congestion_event.last_sample_is_app_limited) {
        return;
    }
    common::BandWidth threshold = full_bw_baseline_ *
            bbr_->params().startup_full_bw_threshold;
    auto cur_max_bw = model_->max_bw();
    if(cur_max_bw >= threshold) {
        full_bw_baseline_ = cur_max_bw;
        rounds_without_bw_growth_ = 0;
    }

    ++rounds_without_bw_growth_;
    full_bw_reached_ = rounds_without_bw_growth_
            >= bbr_->params().startup_full_bw_rounds;
    //TODO: log something
}

void BbrStartupMode::Check_excessive_losses(
        const BbrCongestionEvent& congestion_event)
{
    if (full_bw_reached_) {
        return;
    }
    auto loss_events_in_round = model_->loss_events_in_round();

    // In TCP, loss based exit only happens at end of a loss round.
    // we use the end of the normal round here. It is possible to exit after
    // any congestion event, using information of the "rolling round".
    if (!congestion_event.end_of_round_trip) {
        return;
    }
    // At the end of a round trip. Check if loss is too high in this round.
    if (loss_events_in_round >= bbr_->params().startup_full_loss_count &&
            model_->is_inflight_too_high(congestion_event))
    {
        auto bdp = model_->bdp(model_->max_bw());
        model_->set_inflight_hi(bdp);

        full_bw_reached_ = true;
    }
}

}
