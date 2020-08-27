#ifndef BBR_BANDWIDTH_SAMPLER_H_
#define BBR_BANDWIDTH_SAMPLER_H_

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <vector>
#include <map>
#include <time/time_stamp.h>
#include <common/rate.h>

/**
 * temp
 */
template <class T>
struct MaxFilter
{
  bool operator()(const T& lhs, const T& rhs) const { return lhs >= rhs; }
};

template <class T, class Compare, typename TimeT, typename TimeDeltaT>
class WindowedFilter
{
 public:
  // |window_length| is the period after which a best estimate expires.
  WindowedFilter(TimeDeltaT window_length, T zero_value, TimeT zero_time)
      : window_length_(window_length),
        zero_value_(zero_value),
        estimates_{Sample(zero_value_, zero_time),
                   Sample(zero_value_, zero_time),
                   Sample(zero_value_, zero_time)} {}

  void set_win_len(TimeDeltaT window_length) {
    window_length_ = window_length;
  }
  // Updates best estimates with |sample|, and expires and updates best
  // estimates as necessary.
  void update(T new_sample, TimeT new_time) {
    // Reset all estimates if they have not yet been initialized, if new sample
    // is a new best, or if the newest recorded estimate is too old.
    if (estimates_[0].sample == zero_value_ ||
        Compare()(new_sample, estimates_[0].sample) ||
        new_time - estimates_[2].time > window_length_) {
      Reset(new_sample, new_time);
      return;
    }

    if (Compare()(new_sample, estimates_[1].sample)) {
      estimates_[1] = Sample(new_sample, new_time);
      estimates_[2] = estimates_[1];
    } else if (Compare()(new_sample, estimates_[2].sample)) {
      estimates_[2] = Sample(new_sample, new_time);
    }

    // Expire and update estimates as necessary.
    if (new_time - estimates_[0].time > window_length_) {
      // The best estimate hasn't been updated for an entire window, so promote
      // second and third best estimates.
      estimates_[0] = estimates_[1];
      estimates_[1] = estimates_[2];
      estimates_[2] = Sample(new_sample, new_time);
      // Need to iterate one more time. Check if the new best estimate is
      // outside the window as well, since it may also have been recorded a
      // long time ago. Don't need to iterate once more since we cover that
      // case at the beginning of the method.
      if (new_time - estimates_[0].time > window_length_) {
        estimates_[0] = estimates_[1];
        estimates_[1] = estimates_[2];
      }
      return;
    }
    if (estimates_[1].sample == estimates_[0].sample &&
        new_time - estimates_[1].time > window_length_ >> 2) {
      // A quarter of the window has passed without a better sample, so the
      // second-best estimate is taken from the second quarter of the window.
      estimates_[2] = estimates_[1] = Sample(new_sample, new_time);
      return;
    }

    if (estimates_[2].sample == estimates_[1].sample &&
        new_time - estimates_[2].time > window_length_ >> 1) {
      // We've passed a half of the window without a better estimate, so take
      // a third-best estimate from the second half of the window.
      estimates_[2] = Sample(new_sample, new_time);
    }
  }

  // Resets all estimates to new sample.
  void reset(T new_sample, TimeT new_time) {
    estimates_[0] = estimates_[1] = estimates_[2] =
        Sample(new_sample, new_time);
  }

  T get_best() const { return estimates_[0].sample; }
  T get_second_best() const { return estimates_[1].sample; }
  T get_third_best() const { return estimates_[2].sample; }

 private:
  struct Sample {
    T sample;
    TimeT time;
    Sample(T init_sample, TimeT init_time)
        : sample(init_sample), time(init_time) {}
  };

  TimeDeltaT window_length_;  // Time length of window.
  T zero_value_;              // Uninitialized value of T.
  Sample estimates_[3];       // Best estimate is element 0.
};

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
  uint64_t total_bytes_sent = 0;
  uint64_t total_bytes_acked = 0;
  uint64_t total_bytes_lost = 0;
  uint64_t bytes_in_flight = 0;
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
  // Tracks the maximum number of bytes acked faster than the estimated
  // bandwidth.
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

class BandwidthSampler
{
    struct StateOnTheSentPacket
    {
        size_t bytes;
        time::Timestamp sent_time;

        size_t sent_bytes_at_last_acked_pkt;
        time::Timestamp last_acked_pkt_sent_time;
        time::Timestamp last_acked_pkt_ack_time;

        SendTimeState state;
    };
    struct AckPoint
    {
        time::Timestamp ack_time;
        uint64_t total_bytes_acked = 0;
    };

    class RecentAckPoints
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
    void on_packet_sent(uint64_t seq_no, size_t bytes,
            size_t bytes_in_flight,
            time::Timestamp at_time);
    void on_packets_ack(const std::vector<AckedPacket>& pkts);
    void on_packets_lost(const std::vector<LostPacket>& pkts);

private:
    uint64_t total_bytes_sent_ = 0;
    uint64_t total_bytes_acked_ = 0;
    uint64_t total_bytes_lost_ = 0;
    uint64_t total_bytes_neutered_ = 0;

    uint64_t total_bytes_sent_at_last_acked_packet_ = 0;  //收到这个ACK包的时候，共发出去多少字节

    time::Timestamp last_sent_packet_;
    time::Timestamp last_acked_packet_sent_time_;
    time::Timestamp last_acked_packet_ack_time_;

    // Indicates whether the bandwidth sampler is started in app-limited phase.
    const bool started_as_app_limited_ = true;
    // Indicates whether the bandwidth sampler is currently in an app-limited phase.
    bool is_app_limited_ = false;
    // The packet that will be acknowledged after this one will cause the sampler
    // to exit the app-limited phase.
    uint64_t end_of_app_limited_phase_ = 0;

    std::map<uint64_t/*seq no*/, StateOnTheSentPacket> state_map_;

    RecentAckPoints ack_points_;

    MaxAckHeightTracker max_ack_height_tracker_;

    uint64_t total_bytes_acked_after_last_ack_event_;
};
}

#endif
