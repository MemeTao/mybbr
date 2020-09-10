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

    time::TimeDelta min_rtt() { return min_rtt_;}
    time::Timestamp timestamp() {return min_rtt_timestamp_;}
private:
    time::TimeDelta min_rtt_;
    time::Timestamp min_rtt_timestamp_;
};

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

    float loss_threshold = 0.02; //tcp_bbr2.c

    float startup_cwnd_gain = 2.885;
    float startup_pacing_gain = 2.885;
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
    BbrModel(const Bbrparams& bbr_params,
            time::TimeDelta init_min_rtt,
            time::Timestamp init_min_rtt_timestamp,
            float cwnd_gain,
            float pacing_gain);

    void on_pkt_sent(uint64_t seq_no, size_t pkt_size, size_t infight_bytes,
            time::Timestamp at_time, bool need_retransmitted);

    void on_congestion_event(const std::vector<AckedPacket>& acked_pkts,
            const std::vector<LostPacket>& acked_pkts,
            BbrCongestionEvent& congestion_event,
            time::Timestamp at_time);
    void end_congestion_event(uint64_t least_unacked_pkt_no,
            const BbrCongestionEvent& congestion_event);

    bool is_inflight_too_high( const BbrCongestionEvent& congestion_event);

    size_t bdp(common::BandWidth bw) {
        return bw * rtt_filter_.min_rtt();
    }
    common::BandWidth max_bw() const {
        return bandwidth_filter_.get();
    }
    common::BandWidth bw_lower_bound() const {
        return bw_lower_bound_;
    }
    size_t loss_events_in_round() const {
        return lost_event_in_round_;
    }

    void set_inflight_high_bound(size_t inflight_hi) {
        infight_higth_bound_ = inflight_hi;
    }

    void set_pacing_gain(float gain) {
        pacing_gain_ = gain;
    }
    void set_cwnd_gain(float gain) {
        cwnd_gain_ = gain;
    }

private:
    void adapt_lower_bounds(const BbrCongestionEvent& congestion_event);

private:
    Bbrparams params_;
    float cwnd_gain_;
    float pacing_gain_;

    MinRttFilter rtt_filter_;
    MaxBandwidthFilter bandwidth_filter_;
    RoundTripCounter round_counter_;

    BandwidthSampler sampler_;

    size_t bytes_lost_in_round_ = 0;
    size_t lost_event_in_round_ = 0;

    common::BandWidth latest_max_bw_;
    size_t latest_max_infligth_bytes_ = 0;

    common::BandWidth bw_lower_bound_;
    size_t inflight_lower_bound_;
    size_t infight_higth_bound_;
};

inline size_t bytes_inflight( const SendTimeState& send_state) {
    if (send_state.bytes_in_flight != 0) {
        return send_state.bytes_in_flight;
    }
    return send_state.total_bytes_sent - send_state.total_bytes_acked -
            send_state.total_bytes_lost;
}
}
#endif
