#ifndef BBR_BANDWIDTH_SAMPLER_H_
#define BBR_BANDWIDTH_SAMPLER_H_

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <vector>
#include <deque>
#include <map>
#include <time/time_stamp.h>
#include <common/rate.h>
#include <common/windowed_filter.h>

namespace bbr
{
struct AckedPacket
{
    uint64_t seq_no = 0;
    size_t bytes = 0;
    time::Timestamp receive_time; //the packet received by the peer
};

struct LostPacket
{
    uint64_t seq_no = 0;
    size_t bytes = 0;
};

struct SendTimeState
{
  bool is_valid = false;
  // App limited bandwidth sample might be artificially low because the sender
  // did not have enough data to send in order to saturate the link.
  bool is_app_limited = false;
  size_t total_bytes_sent = 0;
  size_t total_bytes_acked = 0;
  size_t total_bytes_lost = 0;
  size_t bytes_in_flight = 0;
};

class MaxAckHeightTracker
{
public:
    MaxAckHeightTracker(int initial_filter_window)
        :max_ack_height_filter_(initial_filter_window, 0, 0) {}

    uint64_t get() const { return max_ack_height_filter_.get_best(); }

    uint64_t update(common::BandWidth bandwidth_estimate,
                      int round_trip_count,
                      time::Timestamp ack_time,
                      uint64_t bytes_acked);

    void set_win_len(int length) {
        max_ack_height_filter_.set_win_len(length);
    }

    void reset(int new_height, int new_time) {
        max_ack_height_filter_.reset(new_height, new_time);
    }

    void set_threshold(double threshold) {
        threshold_ = threshold;
    }

    double threshold() const {
        return threshold_;
    }

    uint64_t num_ack_aggregation_epochs() const {
        return num_ack_aggregation_epochs_;
    }

private:
    // Tracks the maximum number of bytes acked faster than the estimated bandwidth.
    using MaxAckHeightFilter = WindowedFilter<uint64_t, MaxFilter<uint64_t>, int, int>;
    MaxAckHeightFilter max_ack_height_filter_;
    // The time this aggregation started and the number of bytes acked during it.
    time::Timestamp aggregation_epoch_start_time_;
    uint64_t aggregation_epoch_bytes_ = 0;
    // The number of ack aggregation epochs ever started, including the ongoing
    // one. Stats only.
    uint64_t num_ack_aggregation_epochs_ = 0;
    double threshold_ = 1.0; //tcp_bbr2.c
};

struct CongestionEventSample {
    // The maximum bandwidth sample from all acked packets.
    // QuicBandwidth::Invalid() if no samples are available.
    common::BandWidth sample_max_bandwidth;
    // Whether |sample_max_bandwidth| is from a app-limited sample.
    bool sample_is_app_limited = false;
    // The minimum rtt sample from all acked packets.
    // QuicTime::Delta::Infinite() if no samples are available.
    time::TimeDelta sample_rtt;
    // For each packet p in acked packets, this is the max value of INFLIGHT(p),
    // where INFLIGHT(p) is the number of bytes acked while p is inflight.
    size_t sample_max_inflight = 0;
    // The send state of the largest packet in acked_packets, unless it is
    // empty. If acked_packets is empty, it's the send state of the largest
    // packet in lost_packets.
    SendTimeState last_packet_send_state;
    // The number of extra bytes acked from this ack event, compared to what is
    // expected from the flow's bandwidth. Larger value means more ack
    // aggregation.
    size_t extra_acked = 0;
};

struct BandwidthSample {
    // The bandwidth at that particular sample. 'Invalid' if no valid bandwidth sample
    // is available.
    common::BandWidth bandwidth;

    // The RTT measurement at this particular sample. 'Invalid' if no RTT sample is
    // available.  **Does not correct for delayed ack time**.
    time::TimeDelta rtt;

    // States captured when the packet was sent.
    SendTimeState state_at_send;
};

class BandwidthSampler
{
    struct ConnectionStateOnSentPacket
    {
        size_t bytes;
        time::Timestamp sent_time;

        size_t total_sent_bytes_at_last_acked_pkt;
        time::Timestamp last_acked_pkt_sent_time;
        time::Timestamp last_acked_pkt_ack_time;

        SendTimeState state;
    };
    struct AckPoint
    {
        time::Timestamp ack_time = time::Timestamp::negative_infinity();
        size_t total_bytes_acked = 0;
    };

    class RecentAckPoints //TODO: initializing ack_points[2]
    {
    public:
        void update(time::Timestamp ack_time, uint64_t total_bytes_acked)
        {
            assert(total_bytes_acked >= ack_points_[1].total_bytes_acked);
            if (ack_time < ack_points_[1].ack_time) {
                // This can only happen when time goes backwards, we use the smaller
                // timestamp for the most recent ack point in that case.
                ack_points_[1].ack_time = ack_time;
            } else if (ack_time > ack_points_[1].ack_time) {
                ack_points_[0] = ack_points_[1];
                ack_points_[1].ack_time = ack_time;
            }
            ack_points_[1].total_bytes_acked = total_bytes_acked;
        }
        void clear() { ack_points_[0] = ack_points_[1] = AckPoint(); }
        const AckPoint& recent_point() const { return ack_points_[1]; }
        const AckPoint& less_recent_point() const
        {
            if (ack_points_[0].total_bytes_acked != 0) {
                return ack_points_[0];
            }
            return ack_points_[1];
        }
    private:
        AckPoint ack_points_[2];
    };

public:
    BandwidthSampler(int ack_track_win = 5);
    void on_packet_sent(uint64_t seq_no, size_t bytes,
            size_t bytes_in_flight,
            time::Timestamp at_time, bool need_retransmite = true);
    CongestionEventSample on_congestion_event(time::Timestamp ack_time,
            const std::vector<AckedPacket>& acked_pkts,
            const std::vector<LostPacket>& lost_pkts,
            common::BandWidth max_bw, common::BandWidth estimated_bw_upper_bound,
            int round_count);
    void on_pkt_neutered(uint64_t seq_no);
    void remove_obsolete_pkts(uint64_t up_to); //[0, up_to)
    void on_app_limited();
    bool is_app_limited();
private:
    SendTimeState on_pkt_lost(uint64_t seq_no, size_t bytes);

    BandwidthSample on_pkt_acked(uint64_t seq_no, time::Timestamp ack_time);
private:
    size_t extra_acked(common::BandWidth max_bw, int round_count);
    bool choose_a0(size_t total_bytes_acked, AckPoint& point);
    void connection_state_to_sent_state(const ConnectionStateOnSentPacket& s1,
            SendTimeState& s2);
private:
    uint64_t last_sent_packet_ = std::numeric_limits<uint64_t>::max();

    size_t total_bytes_sent_ = 0;
    size_t total_bytes_acked_ = 0;
    size_t total_bytes_lost_ = 0;
    size_t total_bytes_neutered_ = 0;

    size_t total_bytes_sent_at_last_acked_packet_ = 0;  //在这个被ACK的包发送的时候，共发出去多少字节

    time::Timestamp last_acked_packet_sent_time_;
    time::Timestamp last_acked_packet_ack_time_;

    // Indicates whether the bandwidth sampler is started in app-limited phase.
    const bool started_as_app_limited_ = true;
    // Indicates whether the bandwidth sampler is currently in an app-limited phase.
    bool is_app_limited_ = false;
    // The packet that will be acknowledged after this one will cause the sampler
    // to exit the app-limited phase.
    uint64_t end_of_app_limited_phase_ = std::numeric_limits<uint64_t>::max();

    std::map<uint64_t/*seq no*/, ConnectionStateOnSentPacket> state_map_;

    RecentAckPoints ack_points_;
    std::deque<AckPoint> a0_candidates_;

    MaxAckHeightTracker max_ack_height_tracker_;

    size_t total_bytes_acked_after_last_ack_event_ = 0;
};
}

#endif
