#include <bbr_probe_bw.h>
#include <bbr_algorithm.h>
#include <bbr_model.h>

namespace bbr
{
BbrProbeBandwidth::BbrProbeBandwidth(BbrAlgorithm* bbr,  BbrModel* model)
    :bbr_(bbr),
     model_(model)
{
    ;
}

void BbrProbeBandwidth::enter(time::Timestamp now,
           const BbrCongestionEvent* congestion_event)
{
    if (cycle_.phase == CyclePhase::kProbeNotStarted) {
        // First time entering PROBE_BW. Start a new probing cycle.
        enter_probe_down(/*probed_too_high=*/false,
                /*stopped_risky_probe=*/false, now);
    } else {
        cycle_.cycle_start_time = now;
        if(cycle_.phase == CyclePhase::kPorbeCruise) {
            enter_probe_cruise(now);
        } else if(cycle_.phase == CyclePhase::kProbeRefill) {
            enter_probe_refill(cycle_.probe_up_rounds, now);
        }
    }
}

BbrMode BbrProbeBandwidth::OnCongestionEvent(
    size_t prior_inflight, time::Timestamp at_time,
    const std::vector<AckedPacket>& acked_packets,
    const std::vector<LostPacket>& lost_packets,
    const BbrCongestionEvent& congestion_event)
{
    if (congestion_event.end_of_round_trip) {
        if (cycle_.cycle_start_time != at_time) {
            ++cycle_.rounds_since_probe;
        }
        if (cycle_.phase_start_time != at_time) {
            ++cycle_.rounds_in_phase;
        }
    }

    bool switch_to_probe_rtt = false;

    switch(cycle_.phase) {
    case CyclePhase::kPorbeUp:
        update_probe_up(prior_inflight, congestion_event);
        break;
    case CyclePhase::kPorbeDown:
        update_probe_down(prior_inflight, congestion_event);
        //TODO: change mode when phase changed
        if(cycle_.phase != CyclePhase::kPorbeDown
                && model_->maybe_min_rtt_expired(congestion_event)) {
            switch_to_probe_rtt = true;
        }
        break;
    case CyclePhase::kPorbeCruise:
        update_probe_cruise(congestion_event);
        break;
    case CyclePhase::kProbeRefill:
        update_probe_refill(congestion_event);
        break;
    }

    // Do not need to set the gains if switching to PROBE_RTT, they will be set
    // when BbrProbeRttMode::Enter is called.
    if (!switch_to_probe_rtt) {
        model_->set_pacing_gain(pacing_gain(cycle_.phase));
        model_->set_cwnd_gain(bbr_->params().probe_bw_cwnd_gain);
    }

    return switch_to_probe_rtt ? BbrMode::PROBE_RTT : BbrMode::PROBE_BW;
}

void BbrProbeBandwidth::enter_probe_down(bool probed_too_high,
        bool stopped_risky_probe, time::Timestamp now)
{
    last_cycle_probed_too_high_ = probed_too_high;
    last_cycle_stopped_risky_probe_ = stopped_risky_probe;

    cycle_.cycle_start_time = now;
    cycle_.phase = CyclePhase::kPorbeDown;
    cycle_.rounds_in_phase = 0;
    cycle_.phase_start_time = now;

    //bbr2_pick_probe_wait
    cycle_.rounds_since_probe = bbr_->random().rand<uint32_t>() %
            bbr_->params().bw_probe_rand_rounds;
    cycle_.probe_wait_time = bbr_->params().bbr_bw_probe_base_us +
            bbr_->random().rand<uint32_t>() % bbr_->params().bbr_bw_probe_rand_us;

    cycle_.probe_up_bytes = std::numeric_limits<size_t>::max();
    cycle_.has_advanced_max_bw = false;
    model_->restart_round();
}

void BbrProbeBandwidth::exist_probe_down()
{
    if (!cycle_.has_advanced_max_bw) {
        model_->advance_bw_hi_filter();
        cycle_.has_advanced_max_bw = true;
    }
}

void BbrProbeBandwidth::enter_probe_up(time::Timestamp now)
{
    cycle_.phase = CyclePhase::kPorbeUp;
    cycle_.rounds_in_phase = 0;
    cycle_.phase_start_time = now;
    cycle_.is_sample_from_probing = true;
    raise_inflight_hi();
    model_->restart_round();
}

void BbrProbeBandwidth::enter_probe_cruise(time::Timestamp now)
{
    if(cycle_.phase == CyclePhase::kPorbeDown) {
        exist_probe_down();
    }
    model_->cap_inflight_lo(model_->inflight_hi());
    cycle_.phase = CyclePhase::kPorbeCruise;
    cycle_.rounds_in_phase = 0;
    cycle_.phase_start_time = now;
    cycle_.is_sample_from_probing = false;
}

void BbrProbeBandwidth::enter_probe_refill(uint64_t probe_up_rounds,
        time::Timestamp now)
{
    if(cycle_.phase == CyclePhase::kPorbeDown) {
        exist_probe_down();
    }
    cycle_.phase = CyclePhase::kProbeRefill;
    cycle_.rounds_in_phase = 0;
    cycle_.phase_start_time = now;
    cycle_.is_sample_from_probing = false;
    last_cycle_stopped_risky_probe_ = false;

    model_->clear_bw_lo();
    model_->clear_inflight_lo();
    cycle_.probe_up_rounds = probe_up_rounds;
    cycle_.probe_up_acked = 0;
    model_->restart_round();
}

void BbrProbeBandwidth::update_probe_cruise(
        const BbrCongestionEvent& congestion_event)
{
    ;
}

void BbrProbeBandwidth::raise_inflight_hi()
{
    uint64_t growth_this_round = 1 << cycle_.probe_up_rounds;
    cycle_.probe_up_rounds = std::min<uint64_t>(cycle_.probe_up_rounds + 1, 30);

    size_t probe_up_bytes = bbr_->cwnd() / growth_this_round;
    cycle_.probe_up_bytes =
        std::max<size_t>(probe_up_bytes, Bbrparams::kDefaultTCPMSS);
}

void BbrProbeBandwidth::update_probe_down(size_t prior_inflight,
        const BbrCongestionEvent& congestion_event)
{
    if (cycle_.rounds_in_phase == 1 && congestion_event.end_of_round_trip) {
        /* End of samples from bw probing phase. */
        cycle_.is_sample_from_probing = false;

        if (!congestion_event.last_sample_is_app_limited) {
            /* At this point in the cycle, our current bw sample is also
             * our best recent chance at finding the highest available bw
             * for this flow. So now is the best time to forget the bw
             * samples from the previous cycle, by advancing the window.
             */
            model_->advance_bw_hi_filter();
            cycle_.has_advanced_max_bw = true;
        }

        if (last_cycle_stopped_risky_probe_ && !last_cycle_probed_too_high_) {

            enter_probe_refill(/*probe_up_rounds=*/0, congestion_event.event_time);

            return;
        }
    }

    maybe_adapt_upper_bounds(congestion_event);

    if(is_time_to_probe_bw(congestion_event)) {
        enter_probe_refill(0 /*probe_up_rounds=*/, congestion_event.event_time);
        return;
    }

    if(has_stayed_long_enough_in_probe_down(congestion_event)) {
        enter_probe_cruise(congestion_event.event_time);
        return;
    }

    const size_t inflight_with_headroom =   model_->inflight_hi_with_headroom();
    if (prior_inflight > inflight_with_headroom) {
        // Stay in PROBE_DOWN.
        return;
    }

    // Transition to PROBE_CRUISE if we've drained to target.
    size_t bdp = model_->bdp(model_->max_bw());
    if (prior_inflight < bdp) {
        enter_probe_cruise(congestion_event.event_time);
    }
}

void BbrProbeBandwidth::update_probe_up(size_t prior_inflight,
        const BbrCongestionEvent& congestion_event)
{
    if (maybe_adapt_upper_bounds(congestion_event) ==
            AdaptUpperBoundsResult::kProbedTooHigh)
    {
        enter_probe_down(/*probed_too_high=*/true,
                /*stopped_risky_probe=*/false, congestion_event.event_time);
        return;
    }
    probe_inflight_high_upward(congestion_event);

    bool is_risky = false;
    bool is_queuing = false;

    if (last_cycle_probed_too_high_ &&
            prior_inflight >= model_->inflight_hi()) {
        is_risky = true;
    } else if (cycle_.rounds_in_phase > 0) {
        const size_t bdp = model_->bdp(model_->max_bw());
        size_t queuing_threshold_extra_bytes = 2 * Bbrparams::kDefaultTCPMSS;
        //FIXME:shoud I add ack height into threshold ?
        size_t queuing_threshold = (bbr_->params().probe_bw_probe_inflight_gain * bdp) +
            queuing_threshold_extra_bytes;
        is_queuing = prior_inflight >= queuing_threshold;
    }

    if (is_risky || is_queuing) {
        enter_probe_down(false, is_risky, congestion_event.event_time);
    }
}

void BbrProbeBandwidth::update_probe_cruise(
        const BbrCongestionEvent& congestion_event)
{
    assert(cycle_.phase == CyclePhase::kPorbeCruise);
    maybe_adapt_upper_bounds(congestion_event);
    assert(!cycle_.is_sample_from_probing);

    if (is_time_to_probe_bw(congestion_event)) {
        enter_probe_refill(/*probe_up_rounds=*/0, congestion_event.event_time);
        return;
    }
}

void BbrProbeBandwidth::update_probe_refill(
        const BbrCongestionEvent& congestion_event)
{
    assert(cycle_.phase == CyclePhase::kProbeRefill);
    maybe_adapt_upper_bounds(congestion_event);
    assert(!cycle_.is_sample_from_probing);

    if (cycle_.rounds_in_phase > 0 && congestion_event.end_of_round_trip) {
        enter_probe_up(congestion_event.event_time);
        return;
    }
}

void BbrProbeBandwidth::probe_inflight_high_upward(
        const BbrCongestionEvent& congestion_event)
{
    if (!model_->cwnd_limited(congestion_event)) {
        // Not fully utilizing cwnd, so can't safely grow
        return;
    }

    if (congestion_event.prior_cwnd < model_->inflight_hi()) {
        // Not fully using inflight_hi, so don't grow it.
        return;
    }
    // Increase inflight_hi by the number of probe_up_bytes within probe_up_acked.
    cycle_.probe_up_acked += congestion_event.bytes_acked;
    if (cycle_.probe_up_acked >= cycle_.probe_up_bytes) {
        size_t delta = cycle_.probe_up_acked / cycle_.probe_up_bytes;
        cycle_.probe_up_acked -= delta * cycle_.probe_up_bytes;
        size_t new_inflight_hi = model_->inflight_hi() + delta * Bbrparams::kDefaultTCPMSS;
        if (new_inflight_hi > model_->inflight_hi()) {
            model_->set_inflight_hi(new_inflight_hi);
        }
    }

    if (congestion_event.end_of_round_trip) {
        raise_inflight_hi();
    }
}

BbrProbeBandwidth::AdaptUpperBoundsResult
BbrProbeBandwidth::maybe_adapt_upper_bounds(
        const BbrCongestionEvent& congestion_event)
{
    const SendTimeState& send_state = congestion_event.last_packet_send_state;
    if (!send_state.is_valid) {
        return AdaptUpperBoundsResult::kInvalidSample;
    }
    bool has_enough_loss_events =
        model_->loss_events_in_round() >= bbr_->params().probe_bw_full_loss_count;
    if(model_->is_inflight_too_high(congestion_event) &&
            has_enough_loss_events) //which is not used in tcp-bbr2
    {
        if(cycle_.is_sample_from_probing)
        {
            handle_inflight_too_high(send_state.is_app_limited,
                    bytes_inflight(send_state));
            return AdaptUpperBoundsResult::kProbedTooHigh;
        }
        return AdaptUpperBoundsResult::kOk;
    }

    if(model_->inflight_hi() == BbrModel::kDefaultInflightBytes) {
        return AdaptUpperBoundsResult::kInfilghtHighNotSet;
    }

    const size_t inflight_at_send = bytes_inflight(send_state);
    // Raise the upper bound for inflight.
    if (inflight_at_send > model_->inflight_hi()) {
        //TODO: log something
        model_->set_inflight_hi(inflight_at_send);
    }
    return AdaptUpperBoundsResult::kOk;
}

void BbrProbeBandwidth::handle_inflight_too_high(bool app_limited,
        size_t bytes_inflight)
{
    cycle_.is_sample_from_probing = false;
    if(!app_limited) {
        const size_t inflight_at_send = bytes_inflight(bytes_inflight);
        const size_t inflight_target = bbr_->target_inflight()
                * (1 - bbr_->params().beta);
        if(bbr_->params().limit_inflight_hi_by_cwnd) {
            const size_t cwnd_target = bbr_->cwnd() * (1 - bbr_->params().beta);
            model_->set_inflight_hi(std::max(inflight_at_send, cwnd_target));
        } else {
            //tcp_bbr2.c
            model_->set_inflight_hi(std::max(inflight_at_send, inflight_target));
        }
    }
}

bool BbrProbeBandwidth::is_time_to_probe_bw(
        const BbrCongestionEvent& congestion_event)
{
    if (congestion_event.event_time - cycle_.cycle_start_time >
            cycle_.probe_wait_time)
    {
        return true;
    }

    if (is_time_to_probe_for_reno_coexistence(1.0, congestion_event)) {
        return true;
    }
    return false;
}

bool BbrProbeBandwidth::is_time_to_probe_for_reno_coexistence(
        double probe_wait_fraction,
        const BbrCongestionEvent& congestion_event)
{
    uint64_t rounds = bbr_->params().probe_bw_probe_max_rounds;
    if (bbr_->params().probe_bw_probe_reno_gain > 0.0) {
        size_t target_bytes_inflight = bbr_->target_inflight();
        uint64_t reno_rounds = bbr_->params().probe_bw_probe_reno_gain *
                target_bytes_inflight / Bbrparams::kDefaultTCPMSS;
        rounds = std::min(rounds, reno_rounds);
    }
    bool result = cycle_.rounds_since_probe >= (rounds * probe_wait_fraction);
    //TODO: log something
    return result;
}

bool BbrProbeBandwidth::has_stayed_long_enough_in_probe_down(
        const BbrCongestionEvent& congestion_event)
{
    return congestion_event.event_time - cycle_.cycle_start_time >
        model_->min_rtt();
}

float BbrProbeBandwidth::pacing_gain(CyclePhase phase) const
{
    switch(phase)
    {
    case CyclePhase::kPorbeUp:
        return bbr_->params().probe_bw_probe_up_pacing_gain;
    case CyclePhase::kPorbeDown:
        return bbr_->params().probe_bw_probe_down_pacing_gain;
    default:
        return bbr_->params().probe_bw_default_pacing_gain;
    }
}
}
