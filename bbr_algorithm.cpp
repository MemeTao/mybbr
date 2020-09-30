#include <bbr_algorithm.h>
#include <cassert>

namespace bbr
{
using namespace common::rate;
#define BBR_MODE_DISPATCH(member_function_call)      \
  (cur_mode_ == BbrMode::STARTUP                        \
       ? (mode_start_up_.member_function_call)             \
       : (cur_mode_ == BbrMode::PROBE_BW                \
              ? (mode_probe_bw_.member_function_call)     \
              : (cur_mode_ == BbrMode::DRAIN            \
                     ? (mode_drain_.member_function_call) \
                     : (mode_probe_rtt_.member_function_call))))

// Constants based on TCP defaults.
// The minimum CWND to ensure delayed acks don't reduce bandwidth measurements.
// Does not inflate the pacing rate.
namespace default_params
{
const size_t kMinCwnd= 4 * Bbrparams::kDefaultTCPMSS;

const float kInitialPacingGain = 2.885f;

const int kMaxModeChanges = 4;
}

void BbrAlgorithm::on_packet_sent(uint64_t pkt_no,
        size_t bytes, size_t bytes_in_flight,
        bool need_retransmitted,
        time::Timestamp sent_time)
{
    if(bytes_in_flight == 0) {
        on_exit_quiescence(sent_time);
    }
    model_.on_pkt_sent(pkt_no, bytes, bytes_in_flight,
            sent_time, need_retransmitted);
}

void BbrAlgorithm::on_congestion_event(
    size_t prior_inflight,
    time::Timestamp at_time,
    const std::vector<internal::AckedPacket>& acked_packets,
    const std::vector<internal::LostPacket>& lost_packets)
{
    BbrCongestionEvent congestion_event;
    congestion_event.prior_cwnd = cur_cwnd_;
    congestion_event.prior_bytes_in_flight = prior_inflight;
    congestion_event.is_probing_for_bandwidth =
            BBR_MODE_DISPATCH(is_probing());

    model_.on_congestion_event(acked_packets, lost_packets,
            congestion_event, at_time);

    int mode_changes_allowed = default_params::kMaxModeChanges;
    while(true)
    {
        auto next_mode = BBR_MODE_DISPATCH(
                on_congestion_event(prior_inflight, at_time,
                        acked_packets, lost_packets, congestion_event));
        if (next_mode == cur_mode_) {
            break;
        }

        //TODO: log when mode changed
        BBR_MODE_DISPATCH(leave(at_time, &congestion_event));
        cur_mode_ = next_mode;
        BBR_MODE_DISPATCH(enter(at_time, &congestion_event));
        --mode_changes_allowed;
        if(mode_changes_allowed < 0) {
            //log warning
            break;
        }
    }

    //TODO: implememt it
//    model_.end_congestion_event(uint64_t least_unacked_pkt_no,
//            const BbrCongestionEvent& congestion_event);
    update_pacing_rate(congestion_event.bytes_acked);
    assert(pacing_rate_ > 0_mbps);

    update_cwnd(congestion_event.bytes_acked);
    assert(cur_cwnd_ > 0);

    if (congestion_event.bytes_in_flight == 0) {
        on_exit_quiescence(at_time);
    }
}

size_t BbrAlgorithm::can_send(size_t bytes_inflight) const
{
    return bytes_inflight > cur_cwnd_ ? 0 : cur_cwnd_ - bytes_inflight;
}

void BbrAlgorithm::update_cwnd(size_t bytes_acked)
{
    auto prior_cwnd = cur_cwnd_;
    auto target = target_cwnd(model_.cwnd_gain());
    if (mode_start_up_.full_bw_reached())
    {
        target += model_.max_ack_hegith();
        cur_cwnd_ = std::min(prior_cwnd + bytes_acked, target);
    }
    else if (prior_cwnd < target_cwnd ||
            prior_cwnd < 2 * init_cwnd_)
    {
        cur_cwnd_ = prior_cwnd + bytes_acked;
    }

    auto desired_cwnd = cur_cwnd_;
    cur_cwnd_ = std::min(cur_cwnd_, cwnd_upper_limit());
    auto limitted_cnwd = cur_cwnd_;
    cur_cwnd_ = std::max(cur_cwnd_, min_cwnd());

    (void) desired_cwnd;
    (void) limitted_cnwd;
}

void BbrAlgorithm::update_pacing_rate(size_t bytes_acked)
{
    if(model_.estimated_bw() == 0_mbps) {
        return;
    }

    if (model_.total_bytes_acked() == bytes_acked) {
        // After the first ACK, cwnd_ is still the
        // initial congestion window.
        pacing_rate_ = cur_cwnd_ / model_.min_rtt();
        return;
    }
    auto target_rate = model_.pacing_gain() * model_.estimated_bw();

    if (mode_start_up_.full_bw_reached()) {
        pacing_rate_ = target_rate;
        return;
    }

    if (target_rate > pacing_rate_) {
        pacing_rate_ = target_rate;
    }
}

size_t BbrAlgorithm::target_cwnd(float gain)
{
    return std::max(model_.bdp(model_.estimated_bw(), gain),
            min_cwnd());
}

size_t BbrAlgorithm::cwnd_upper_limit()
{
    auto upper_limit_by_mode = BBR_MODE_DISPATCH(cwnd_upper_limit());
    return upper_limit_by_mode;
}

void BbrAlgorithm::on_exit_quiescence(time::Timestamp at_time)
{
    if(!last_quiescence_start_.is_valid()) {
        return;
    }

    auto next_mode = BBR_MODE_DISPATCH( on_exit_quiescence(
            std::min(at_time, last_quiescence_start_), at_time));
    if (next_mode != cur_mode_) {
        BBR_MODE_DISPATCH(leave(at_time, nullptr));
        cur_mode_ = next_mode;
        BBR_MODE_DISPATCH(enter(at_time, nullptr));
    }
    last_quiescence_start_ = time::Timestamp::positive_infinity();
}
}

