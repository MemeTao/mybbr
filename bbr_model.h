#ifndef BBR_MODEL_H_
#define BBR_MODEL_H_

#include <bandwidth_sampler.h>
#include <round_trip_counter.h>

namespace bbr
{
class MinRttFilter
{
public:
    MinRttFilter(time::TimeDelta initial_min_rtt,
            time::Timestamp initial_min_rtt_timestamp)
        : min_rtt_(initial_min_rtt),
        min_rtt_timestamp_(initial_min_rtt_timestamp)
    {}

    void update(time::TimeDelta sample_rtt, time::Timestamp at_time);
    void force_update(time::TimeDelta sample_rtt, time::Timestamp at_time);

    time::TimeDelta min_rtt() const { return min_rtt_;}
    time::Timestamp timestamp() const {return min_rtt_timestamp_;}

private:
    time::TimeDelta min_rtt_;
    time::Timestamp min_rtt_timestamp_;
};

/* Keep max of last 1-2 cycles.*/
class MaxBandwidthFilter
{
public:
    void update(common::BandWidth sample) {
        max_bw_[1] = std::max(sample, max_bw_[1]);
    }

    void advance()
    {
        if (max_bw_[1] == common::BandWidth{0}) {
          return;
        }
        max_bw_[0] = max_bw_[1];
        max_bw_[1] = common::BandWidth{0};
    }

    common::BandWidth get() const {
        return std::max(max_bw_[0], max_bw_[1]);
    }
private:
    common::BandWidth max_bw_[2] = {{0}, {0}};
};

struct Bbrparams
{
    // A common factor for multiplicative decreases. Used for adjusting
    // bandwidth_lo, inflight_lo and inflight_hi upon losses.
    float beta = 0.3;

    bool ignore_inflight_lo = false;

    // Full bandwidth is declared if the total bandwidth growth is less than
    // |startup_full_bw_threshold| times in the last |startup_full_bw_rounds|
    // round trips.
    float startup_full_bw_threshold = 1.25;

    uint64_t startup_full_bw_rounds = 3;

    uint8_t startup_full_loss_count = 8; //tcp_bbr2.c

    uint8_t probe_bw_full_loss_count = 2; //quic-bbr2

    float loss_threshold = 0.02; //tcp_bbr2.c

    float startup_cwnd_gain = 2.885;
    float startup_pacing_gain = 2.885;

    float drain_cwnd_gain = 2.885;
    float drain_pacing_gain = 1.0 / 2.885;

    //probe bandwidth gains
    //cwnd gains
    float probe_bw_cwnd_gain = 2.0;

    // Multiplier to get target inflight (as multiple of BDP) for PROBE_UP phase.
    float probe_bw_probe_inflight_gain = 1.25;

    // Pacing gains.
    float probe_bw_probe_up_pacing_gain = 1.25;
    float probe_bw_probe_down_pacing_gain = 0.75;
    float probe_bw_default_pacing_gain = 1.0;

    //amount of randomness to inject in round counting for Reno-coexistence.
    uint8_t bw_probe_rand_rounds = 2; //tcp_bbr2.c
    uint32_t bbr_bw_probe_base_us = 2 * 1000 * 1000; // 2 sec tcp_bbr2.c
    uint32_t bbr_bw_probe_rand_us = 1 * 1000 * 1000; //1 sec tcp_bbr2.c

    bool limit_inflight_hi_by_cwnd = false;

    float inflight_hi_headroom_fraction = 0.01; //tcp_bbr2:15%, quic_bbr2: 1%

    time::TimeDelta min_rtt_win {10 * 1000 * 1000};

    const static size_t kDefaultTCPMSS = 1460;

    uint8_t probe_bw_probe_max_rounds = 63;

    // Multiplier to get Reno-style probe epoch duration as: k * BDP round trips.
    // If zero, disables Reno-style BDP-scaled coexistence mechanism.
    float probe_bw_probe_reno_gain = 1.0;

    //probe rtt:200ms
    time::TimeDelta probe_rtt_duration {200 * 1000};
    float probe_rtt_inflight_target_bdp_fraction = 0.5;

    size_t min_cwnd = 4 * kDefaultTCPMSS;
};

// Information that are meaningful only when Bbr2Sender::OnCongestionEvent is
// running.
struct BbrCongestionEvent
{
    // The congestion window prior to the processing of the ack/loss events.
    size_t prior_cwnd = 0;

    // Total bytes inflight before the processing of the ack/loss events.
    size_t prior_bytes_in_flight = 0;

    // Total bytes inflight after the processing of the ack/loss events.
    size_t bytes_in_flight = 0;

    // Total bytes acked from acks in this event.
    size_t bytes_acked = 0;

    // Total bytes lost from losses in this event.
    size_t bytes_lost = 0;

    // Whether acked_packets indicates the end of a round trip.
    bool end_of_round_trip = false;

    // TODO(wub): After deprecating --quic_one_bw_sample_per_ack_event, use
    // last_packet_send_state.is_app_limited instead of this field.
    // Whether the last bandwidth sample from acked_packets is app limited.
    // false if acked_packets is empty.
    bool last_sample_is_app_limited = false;

    // When the event happened, whether the sender is probing for bandwidth.
    bool is_probing_for_bandwidth = false;

    // Minimum rtt of all bandwidth samples from acked_packets.
    // QuicTime::Delta::Infinite() if acked_packets is empty.
    time::TimeDelta sample_min_rtt;

    // Maximum bandwidth of all bandwidth samples from acked_packets.
    common::BandWidth sample_max_bandwidth;

    // The send state of the largest packet in acked_packets, unless it is empty.
    // If acked_packets is empty, it's the send state of the largest packet in
    // lost_packets.
    SendTimeState last_packet_send_state;

    time::Timestamp event_time;
};

class BbrModel
{
public:
    const static size_t kDefaultInflightBytes = std::numeric_limits<size_t>::max();
public:
    BbrModel(const Bbrparams& bbr_params,
            time::TimeDelta init_min_rtt,
            time::Timestamp init_min_rtt_timestamp,
            float cwnd_gain,
            float pacing_gain);

    void on_pkt_sent(uint64_t seq_no, size_t pkt_size, size_t infight_bytes,
            time::Timestamp at_time, bool need_retransmitted);

    void on_congestion_event(const std::vector<internal::AckedPacket>& acked_pkts,
            const std::vector<internal::LostPacket>& lost_pkts,
            BbrCongestionEvent& congestion_event,
            time::Timestamp at_time);
    void end_congestion_event(uint64_t least_unacked_pkt_no,
            const BbrCongestionEvent& congestion_event);

    void postpone_min_rtt_timestamp(time::TimeDelta duration);

    bool is_inflight_too_high( const BbrCongestionEvent& congestion_event);

    bool maybe_min_rtt_expired(const BbrCongestionEvent& congestion_event);

    time::TimeDelta min_rtt() const { return rtt_filter_.min_rtt();}

    common::BandWidth max_bw() const{ return bandwidth_filter_.get();}

    common::BandWidth estimated_bw() const { return std::min(max_bw(), bw_lo_);}

    size_t bdp(common::BandWidth bw, float gain = 1.0) const
    {
        return bw * (min_rtt() * gain);
    }

    common::BandWidth bw_lower_bound() const { return bw_lo_;}

    size_t loss_events_in_round() const{ return lost_event_in_round_;}

    size_t inflight_lo() const { return inflight_lo_;}

    size_t inflight_hi() const { return inflight_hi_;}

    size_t inflight_hi_with_headroom() const;

    bool cwnd_limited( const BbrCongestionEvent& congestion_event) const;

    size_t total_bytes_acked() const { return sampler_.total_bytes_acked();}

    size_t max_ack_hegith() const { return sampler_.max_ack_height();}
public:
    void set_inflight_hi(size_t inflight_hi){ inflight_hi_ = inflight_hi;}

    void cap_inflight_lo(size_t cap);

    void clear_inflight_lo();

    void clear_bw_lo();

    void restart_round();

    void set_pacing_gain(float gain) { pacing_gain_ = gain;}
    void set_cwnd_gain(float gain) { cwnd_gain_ = gain;}
    void advance_bw_hi_filter() { bandwidth_filter_.advance();}

    float pacing_gain() const { return pacing_gain_;}
    float cwnd_gain() const { return cwnd_gain_;}

private:
    void adapt_lower_bounds(const BbrCongestionEvent& congestion_event);

private:
    Bbrparams params_;
    float cwnd_gain_;
    float pacing_gain_;

    MinRttFilter rtt_filter_;
    //The filter that tracks the maximum bandwidth over
    //multiple recent round trips.
    MaxBandwidthFilter bandwidth_filter_;
    RoundTripCounter round_counter_;

    BandwidthSampler sampler_;

    size_t bytes_lost_in_round_ = 0;
    size_t lost_event_in_round_ = 0;

    // Max bandwidth in the current round. Updated once per congestion event.
    common::BandWidth latest_max_bw_;
    //current round
    size_t latest_max_infligth_bytes_ = 0;

    // Max bandwidth of recent rounds. Updated once per round.
    common::BandWidth bw_lo_;
    //recent rounds
    size_t inflight_lo_;
    size_t inflight_hi_;
};

inline size_t bytes_inflight( const SendTimeState& send_state)
{
    if (send_state.bytes_in_flight != 0) {
        return send_state.bytes_in_flight;
    }
    return send_state.total_bytes_sent - send_state.total_bytes_acked -
            send_state.total_bytes_lost;
}
}
#endif
