#include <bbr_model.h>

namespace bbr
{
using namespace common::rate;

void MinRttFilter::update(time::TimeDelta sample_rtt,
        time::Timestamp at_time)
{
    if (sample_rtt < min_rtt_ || !min_rtt_timestamp_.is_valid()) {
        min_rtt_ = sample_rtt;
        min_rtt_timestamp_ = at_time;
    }
}

void MinRttFilter::force_update(time::TimeDelta sample_rtt,
        time::Timestamp at_time)
{
    min_rtt_ = sample_rtt;
    min_rtt_timestamp_ = at_time;
}

BbrModel::BbrModel(const Bbrparams& bbr_params,
        time::TimeDelta init_min_rtt,
        time::Timestamp init_min_rtt_timestamp,
        float cwnd_gain,
        float pacing_gain)
    :params_(bbr_params),
     rtt_filter_(init_min_rtt, init_min_rtt_timestamp),
     cwnd_gain_(cwnd_gain),
     pacing_gain_(pacing_gain),
     latest_max_bw_(0_mbps),
     bw_lo_(common::BandWidth::positive_infinity()),
     inflight_lo_(kDefaultInflightBytes),
     inflight_hi_(kDefaultInflightBytes)
{
    ;
}

void BbrModel::on_pkt_sent(uint64_t seq_no, size_t pkt_size,
        size_t infight_bytes, time::Timestamp at_time,
        bool need_retransmitted)
{
    round_counter_.on_pkt_sent(seq_no);
    sampler_.on_packet_sent(seq_no, pkt_size, infight_bytes, at_time,
            need_retransmitted);
}

void BbrModel::on_congestion_event(
        const std::vector<internal::AckedPacket>& acked_pkts,
        const std::vector<internal::LostPacket>& lost_pkts,
        BbrCongestionEvent& congestion_event,
        time::Timestamp at_time)
{
    size_t prior_acked = sampler_.total_bytes_acked();
    size_t prior_lost = sampler_.total_bytes_lost();

    congestion_event.event_time = at_time;

    if(!acked_pkts.empty()) {
        congestion_event.end_of_round_trip =
            round_counter_.on_pkt_acked(acked_pkts.rbegin()->seq_no);
    }

    auto sample = sampler_.on_congestion_event(at_time, acked_pkts, lost_pkts,
            max_bw(), bw_lower_bound(), round_counter_.count());
    if (sample.last_packet_send_state.is_valid) {
        congestion_event.last_packet_send_state = sample.last_packet_send_state;
        congestion_event.last_sample_is_app_limited =
                sample.last_packet_send_state.is_app_limited;
    }

    // Avoid updating |max_bandwidth_filter_| if
    // a) this is a loss-only event, or
    // b) all packets in |acked_packets| did not generate valid samples.
    //    (e.g. ack of ack-only packets).
    // In both cases, total_bytes_acked() will not change.
    if (prior_acked != sampler_.total_bytes_acked()) {
        if (!sample.sample_is_app_limited ||
                sample.sample_max_bandwidth > max_bw()) {
            congestion_event.sample_max_bandwidth = sample.sample_max_bandwidth;
            bandwidth_filter_.update(congestion_event.sample_max_bandwidth);
        }
    }

    if (sample.sample_rtt.is_valid()) {
        congestion_event.sample_min_rtt = sample.sample_rtt;
        rtt_filter_.update(congestion_event.sample_min_rtt, at_time);
    }

    congestion_event.bytes_acked = sampler_.total_bytes_acked() - prior_acked;
    congestion_event.bytes_lost = sampler_.total_bytes_lost() - prior_lost;

    if (congestion_event.prior_bytes_in_flight >=
            congestion_event.bytes_acked + congestion_event.bytes_lost) {
        congestion_event.bytes_in_flight =
                congestion_event.prior_bytes_in_flight -
                congestion_event.bytes_acked - congestion_event.bytes_lost;
    } else {
        //shoudn't happen
        //TODO: log something
        congestion_event.bytes_in_flight = 0;
    }

    if(congestion_event.bytes_lost > 0) {
        bytes_lost_in_round_ += congestion_event.bytes_lost;
        lost_event_in_round_ ++;
    }

    //latest_max_bw_\latest_max_infligth_bytes_ only increased within a round
    latest_max_bw_ = std::max(latest_max_bw_, sample.sample_max_bandwidth);
    latest_max_infligth_bytes_ = std::max(latest_max_infligth_bytes_,
            sample.sample_max_inflight);

    if(!congestion_event.end_of_round_trip) {
        return;
    }

    adapt_lower_bounds(congestion_event);

    if(sample.sample_max_bandwidth > 0_mbps) {
        latest_max_bw_ = sample.sample_max_bandwidth;
    }

    if(sample.sample_max_inflight > 0) {
        latest_max_infligth_bytes_ = sample.sample_max_inflight;
    }
}

void BbrModel::end_congestion_event(
        uint64_t least_unacked_pkt_no,
        const BbrCongestionEvent& congestion_event)
{
    if (congestion_event.end_of_round_trip) {
        bytes_lost_in_round_ = 0;
        lost_event_in_round_ = 0;
    }
    sampler_.remove_obsolete_pkts(least_unacked_pkt_no);
}

void BbrModel::restart_round()
{
    bytes_lost_in_round_ = 0;
    lost_event_in_round_ = 0;
    round_counter_.restart();
}

void BbrModel::adapt_lower_bounds(const BbrCongestionEvent& congestion_event)
{
    if(!congestion_event.end_of_round_trip
            || congestion_event.is_probing_for_bandwidth) {
        return;
    }
    //TODO:log bounds change
    if(congestion_event.bytes_lost > 0)
    {
        if(!bw_lo_.is_valid()) {
            bw_lo_ = max_bw();
        }
        bw_lo_ = std::max(latest_max_bw_, bw_lo_ * (1.0 - params_.beta));
        if(params_.ignore_inflight_lo) {
            return;
        }
        if (inflight_lo_ == std::numeric_limits<size_t>::max()) {
            inflight_lo_ = congestion_event.prior_cwnd;
        }
        inflight_lo_ = std::max(latest_max_infligth_bytes_,
                static_cast<size_t>(inflight_lo_ * (1.0 - params_.beta)));
    }
}

bool BbrModel::is_inflight_too_high(const BbrCongestionEvent& congestion_event)
{
    const SendTimeState& send_state = congestion_event.last_packet_send_state;
    if(!send_state.is_valid) {
        return false;
    }
    size_t inflight_at_send = bytes_inflight(send_state);
    size_t bytes_lost_in_round = bytes_lost_in_round_;

    if(inflight_at_send > 0 && bytes_lost_in_round > 0) {
        size_t lost_in_round_threshold =
            inflight_at_send * params_.loss_threshold;
        return bytes_lost_in_round > lost_in_round_threshold;
    }
    return false;
}

void BbrModel::cap_inflight_lo(size_t cap)
{
    if (params_.ignore_inflight_lo) {
        return;
    }

    if (inflight_lo_ != kDefaultInflightBytes && inflight_lo_ > cap) {
        inflight_lo_ = cap;
    }
}

size_t BbrModel::inflight_hi_with_headroom() const
{
    size_t headroom = inflight_hi_ * params_.inflight_hi_headroom_fraction;

    return inflight_hi_ > headroom ? inflight_hi_ - headroom : 0;
}

bool BbrModel::maybe_min_rtt_expired(const BbrCongestionEvent& congestion_event)
{
    if(!rtt_filter_.timestamp().is_valid() || congestion_event.event_time <
            rtt_filter_.timestamp() + params_.min_rtt_win) {
        return false;
    }
    if( !congestion_event.sample_min_rtt.is_valid()) {
        return false;
    }
    rtt_filter_.force_update(congestion_event.sample_min_rtt, congestion_event.event_time);
    return true;
}

bool BbrModel::cwnd_limited(const BbrCongestionEvent& congestion_event) const
{
    size_t prior_bytes_in_flight = congestion_event.bytes_in_flight +
                congestion_event.bytes_acked + congestion_event.bytes_lost;
    return prior_bytes_in_flight >= congestion_event.prior_cwnd;
}

void BbrModel::postpone_min_rtt_timestamp(time::TimeDelta duration)
{
    rtt_filter_.force_update(min_rtt(), rtt_filter_.timestamp() + duration);
}

}
