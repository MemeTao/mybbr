#include <bandwidth_sampler.h>

namespace bbr
{

uint64_t MaxAckHeightTracker::update(common::BandWidth bw,
        int round_trip_count, time::Timestamp ack_time, uint64_t bytes_acked)
{
    if (!aggregation_epoch_start_time_.is_valid()) {
        aggregation_epoch_bytes_ = bytes_acked;
        aggregation_epoch_start_time_ = ack_time;
        ++num_ack_aggregation_epochs_;
        return 0;
    }

    // Compute how many bytes are expected to be delivered, assuming bw is correct.
    size_t expected_bytes_acked = bw * (ack_time - aggregation_epoch_start_time_) / 8;
    // Reset the current aggregation epoch as soon as the ack arrival rate is less
    // than or equal to the max bandwidth.
    if (aggregation_epoch_bytes_ <= threshold_ * expected_bytes_acked) {
        // Reset to start measuring a new aggregation epoch.
        aggregation_epoch_bytes_ = bytes_acked;
        aggregation_epoch_start_time_ = ack_time;
        ++num_ack_aggregation_epochs_;
        return 0;
    }

    aggregation_epoch_bytes_ += bytes_acked;

    // Compute how many extra bytes were delivered vs max bandwidth.
    size_t extra_bytes_acked = aggregation_epoch_bytes_ - expected_bytes_acked;
    max_ack_height_filter_.update(extra_bytes_acked, round_trip_count);
    return extra_bytes_acked;
}

BandwidthSampler::BandwidthSampler(int ack_track_win)
    :max_ack_height_tracker_(ack_track_win)
{
    max_ack_height_tracker_.set_threshold(2.0);
}

void BandwidthSampler::on_packet_sent(uint64_t seq_no, size_t bytes,
        size_t bytes_in_flight, time::Timestamp at_time, bool need_retransmite)
{
    last_sent_packet_ = seq_no;
    if(!need_retransmite) {
        return ;
    }

    total_bytes_sent_ += bytes;

    if(bytes_in_flight == 0) {
        //assume we received last ack at this moment
        last_acked_packet_ack_time_ = at_time;
        total_bytes_sent_at_last_acked_packet_ = total_bytes_sent_;
        // In this situation ack compression is not a concern, set send rate to
        // effectively infinite.
        last_acked_packet_sent_time_ = at_time;

        ack_points_.clear();
        ack_points_.update(at_time, total_bytes_acked_);

        a0_candidates_.clear();
        a0_candidates_.push_back(ack_points_.recent_point());
    }

    SendTimeState cur_state {
        false,  //invalid
        is_app_limited_,
        total_bytes_sent_,
        total_bytes_acked_,
        total_bytes_lost_,
        bytes + bytes_in_flight};

    state_map_.insert(std::make_pair(seq_no,
            ConnectionStateOnSentPacket{
                bytes,
                at_time,
                total_bytes_sent_at_last_acked_packet_,
                last_acked_packet_sent_time_,
                last_acked_packet_ack_time_,
                cur_state}));
    //TODO: warn when state_map_ contain too much tracked packet
}

CongestionEventSample BandwidthSampler::on_congestion_event(time::Timestamp ack_time,
        const std::vector<AckedPacket>& acked_pkts,
        const std::vector<LostPacket>& lost_pkts,
        common::BandWidth max_bw,
        common::BandWidth estimated_bw_upper_bound,
        int round_count)
{
    CongestionEventSample event_sample;

    SendTimeState last_lost_packet_send_state;

    for (const LostPacket& pkt : lost_pkts) {
        SendTimeState send_state = on_pkt_lost(pkt.seq_no, pkt.bytes);
        if (send_state.is_valid) {
            last_lost_packet_send_state = send_state;
        }
    }

    if (acked_pkts.empty()) {
      // Only populate send state for a loss-only event.
      event_sample.last_packet_send_state = last_lost_packet_send_state;
      return event_sample;
    }

    SendTimeState last_acked_packet_send_state;
    for (const auto& pkt : acked_pkts) {
        BandwidthSample sample =  on_pkt_acked( pkt.seq_no, ack_time);
        if (!sample.state_at_send.is_valid) {
            continue;
        }

        last_acked_packet_send_state = sample.state_at_send;
        //get minimum rtt
        if (sample.rtt.is_valid()) {
            event_sample.sample_rtt = std::min(event_sample.sample_rtt, sample.rtt);
        }
        //get maximum bandwidth
        if (sample.bandwidth.is_valid() && (!event_sample.sample_max_bandwidth.is_valid()
                || sample.bandwidth > event_sample.sample_max_bandwidth)) {
            event_sample.sample_max_bandwidth = sample.bandwidth;
            event_sample.sample_is_app_limited = sample.state_at_send.is_app_limited;
        }
        //get maximum inflight
        size_t inflight_sample = total_bytes_acked_ - last_acked_packet_send_state.total_bytes_acked;
        if (inflight_sample > event_sample.sample_max_inflight) {
            event_sample.sample_max_inflight = inflight_sample;
        }
    }

    if (!last_lost_packet_send_state.is_valid)
    {
        event_sample.last_packet_send_state = last_acked_packet_send_state;
    }
    else if (!last_acked_packet_send_state.is_valid)
    {
        event_sample.last_packet_send_state = last_lost_packet_send_state;
    }
    else
    {
        // If two packets are inflight and an alarm is armed to lose a packet and it
        // wakes up late, then the first of two in flight packets could have been
        // acknowledged before the wakeup, which re-evaluates loss detection, and
        // could declare the later of the two lost.
        event_sample.last_packet_send_state = lost_pkts.back().seq_no > acked_pkts.back().seq_no
              ? last_lost_packet_send_state : last_acked_packet_send_state;
    }

    max_bw = std::max(max_bw, event_sample.sample_max_bandwidth);
    max_bw = std::min(max_bw, estimated_bw_upper_bound);

    event_sample.extra_acked = extra_acked(max_bw, round_count);

    return event_sample;
}

SendTimeState BandwidthSampler::on_pkt_lost(uint64_t seq_no, size_t bytes)
{
    SendTimeState state;

    total_bytes_lost_ += bytes;
    auto iter = state_map_.find(seq_no);
    if(iter != state_map_.end())
    {
        connection_state_to_sent_state(iter->second, state);
    }
    return state;
}

BandwidthSample BandwidthSampler::on_pkt_acked(uint64_t seq_no, time::Timestamp ack_time)
{
    BandwidthSample sample;
    auto iter = state_map_.find(seq_no);
    if(iter ==  state_map_.end()) {
        return sample;
    }
    ConnectionStateOnSentPacket& sent_pkt = iter->second;

    total_bytes_acked_ += sent_pkt.bytes;
    total_bytes_sent_at_last_acked_packet_ = sent_pkt.state.total_bytes_sent;
    last_acked_packet_sent_time_ = sent_pkt.sent_time;
    last_acked_packet_ack_time_ = ack_time;

    ack_points_.update(ack_time, total_bytes_acked_);

    if(is_app_limited_) {
        // Exit app-limited phase in two cases:
        // (1) end_of_app_limited_phase_ is not initialized, i.e., so far all
        // packets are sent while there are buffered packets or pending data.
        // (2) The current acked packet is after the sent packet marked as the end
        // of the app limit phase.
        if (end_of_app_limited_phase_ == std::numeric_limits<uint64_t>::max()
                || seq_no > end_of_app_limited_phase_)
        {
            is_app_limited_ = false;
        }
    }
    // There might have been no packets acknowledged at the moment when the
    // current packet was sent. In that case, there is no bandwidth sample to
    // make.
    if(!sent_pkt.last_acked_pkt_sent_time.is_valid()) {
        return sample;
    }
    common::BitRate send_rate;
    if (sent_pkt.sent_time > sent_pkt.last_acked_pkt_sent_time) {
        send_rate =  (sent_pkt.state.total_bytes_sent - sent_pkt.total_sent_bytes_at_last_acked_pkt) /
              (sent_pkt.sent_time - sent_pkt.last_acked_pkt_sent_time);
    }
    AckPoint a0;
    if(!choose_a0(sent_pkt.state.total_bytes_acked, a0)) {
        a0.ack_time = sent_pkt.last_acked_pkt_ack_time;
        a0.total_bytes_acked = sent_pkt.state.total_bytes_acked;
    }
    assert(a0.ack_time < ack_time);
    common::BitRate ack_rate = (total_bytes_acked_ - a0.total_bytes_acked) / (ack_time - a0.ack_time);

    sample.bandwidth = std::min(send_rate, ack_rate);
    sample.rtt = ack_time - sent_pkt.sent_time;
    connection_state_to_sent_state(sent_pkt, sample.state_at_send);

    return sample;
}

size_t BandwidthSampler::extra_acked(common::BandWidth max_bw, int round_count)
{
    size_t newly_acked_bytes = total_bytes_acked_ -
            total_bytes_acked_after_last_ack_event_;
    if (newly_acked_bytes == 0) {
        return 0;
    }
    total_bytes_acked_after_last_ack_event_ = total_bytes_acked_;
    size_t extra_acked = max_ack_height_tracker_.update(max_bw, round_count,
            last_acked_packet_ack_time_, newly_acked_bytes);
    if(extra_acked == 0) {
        a0_candidates_.push_back(ack_points_.less_recent_point());
    }
    return extra_acked;
}

bool BandwidthSampler::choose_a0(size_t total_bytes_acked, AckPoint& point)
{
    if (a0_candidates_.empty()) {
      return false;
    }

    if (a0_candidates_.size() == 1) {
        point = a0_candidates_.front();
        return true;
    }

    for (size_t i = 1; i < a0_candidates_.size(); ++i) {
        if (a0_candidates_[i].total_bytes_acked > total_bytes_acked) {
            point = a0_candidates_[i-1];
            while (i > 1) {
                a0_candidates_.pop_front();
                i--;
            }
            return true;
        }
    }
    // All candidates' total_bytes_acked is <= |total_bytes_acked|.
    point = a0_candidates_.back();
    while(a0_candidates_.size() > 1) {
        a0_candidates_.pop_front();
    }
    return true;
}

void BandwidthSampler::on_pkt_neutered(uint64_t seq_no)
{
    auto iter = state_map_.find(seq_no);
    if(iter == state_map_.end()) {
        return;
    }
    total_bytes_neutered_ += iter->second.bytes;
    state_map_.erase(iter);
}
//[0, up_to)
void BandwidthSampler::remove_obsolete_pkts(uint64_t up_to)
{
    auto iter = state_map_.lower_bound(up_to);
    if(iter != state_map_.end()) {
        state_map_.erase(state_map_.begin(), iter);
    }
}

void BandwidthSampler::on_app_limited()
{
    is_app_limited_ = true;
    end_of_app_limited_phase_ = last_sent_packet_;
}

bool BandwidthSampler::is_app_limited()
{
    return is_app_limited_;
}

void BandwidthSampler::connection_state_to_sent_state(
        const ConnectionStateOnSentPacket& s1, SendTimeState& s2)
{
    s2 = s1.state;
    s2.is_valid = true;
}
}
