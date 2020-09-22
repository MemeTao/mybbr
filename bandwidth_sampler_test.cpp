#include <gtest/gtest.h>
#include <time/timestamp.h>
#include <common/rate.h>
#include <bandwidth_sampler.h>

using BandWidth = bbr::common::BandWidth;
using BandwidthSample = bbr::BandwidthSample;
using namespace bbr::common::rate;
using namespace bbr::time;
const size_t kRegularPktSize = 1280;

class BandwidthSamplerTest : public testing::Test{
public:
    BandwidthSamplerTest()
        :sampler_(0),
         app_limited_at_start_(sampler_.is_app_limited())
    {}
    void SetUp()
    {
        clock_ = 0;
    }
    void send_pkt(uint64_t seq_no);
    bbr::CongestionEventSample on_congestion_event(
            std::set<uint64_t> ack, std::set<uint64_t> lost);
    BandWidth ack_pkt(uint64_t seq_no);
    bbr::SendTimeState lose_pkt(uint64_t seq_no);
    void send_40pkts_and_ack_first20pkts(bbr::time::TimeDelta dt);
protected:
    void send_pkt_inner(uint64_t seq_no, size_t bytes,
            bool need_retransmitted);
    BandwidthSample ack_pkt_inner(uint64_t seq_no);

    bbr::time::Timestamp clock_;
    bbr::BandwidthSampler sampler_;
    size_t bytes_in_flight_ = 0;
    BandWidth max_bw_ = 0_mbps;
    BandWidth bw_upper_bound_ = 1000_mbps;
    int round_count_ = 0;
    bool app_limited_at_start_;

    std::map<uint64_t/*seq_no*/, size_t /*pkt size*/> sent_pkts_;
};

void BandwidthSamplerTest::send_pkt(uint64_t seq_no)
{
    send_pkt_inner(seq_no, kRegularPktSize, true);
}

BandWidth BandwidthSamplerTest::ack_pkt(uint64_t seq_no)
{
    auto sample = ack_pkt_inner(seq_no);
    return sample.bandwidth;
}

void BandwidthSamplerTest::send_pkt_inner(uint64_t seq_no,
        size_t bytes, bool need_retransmitted)
{
    sampler_.on_packet_sent(seq_no, kRegularPktSize,
            bytes_in_flight_, clock_, true);
    if(need_retransmitted) {
        bytes_in_flight_ += bytes;
    }
    sent_pkts_.insert(std::make_pair(seq_no, bytes));
}

BandwidthSample BandwidthSamplerTest::ack_pkt_inner(uint64_t seq_no)
{
    size_t size = sent_pkts_.find(seq_no)->second;
    bytes_in_flight_ -= size;
    bbr::AckedPacket acked_pkt{seq_no, size, clock_};
    auto cong_sample = sampler_.on_congestion_event(clock_, {acked_pkt},
            {}, max_bw_, bw_upper_bound_, round_count_);
    max_bw_ = std::max(cong_sample.sample_max_bandwidth, max_bw_);

    BandwidthSample bw_sample;
    bw_sample.bandwidth = cong_sample.sample_max_bandwidth;
    bw_sample.rtt = cong_sample.sample_rtt;
    bw_sample.state_at_send = cong_sample.last_packet_send_state;
    EXPECT_TRUE(bw_sample.state_at_send.is_valid);

    return bw_sample;
}

bbr::SendTimeState BandwidthSamplerTest::lose_pkt(uint64_t seq_no)
{
    size_t size = sent_pkts_.find(seq_no)->second;
    bytes_in_flight_ -= size;
    bbr::LostPacket lost_pkt{seq_no, size};
    auto sample = sampler_.on_congestion_event(clock_, {}, {lost_pkt},
            max_bw_, bw_upper_bound_, round_count_);
    EXPECT_TRUE(sample.last_packet_send_state.is_valid);
    EXPECT_EQ(sample.sample_max_bandwidth, 0_mbps);
    EXPECT_FALSE(sample.sample_rtt.is_valid());
    return sample.last_packet_send_state;
}

bbr::CongestionEventSample BandwidthSamplerTest::on_congestion_event(
        std::set<uint64_t> ack, std::set<uint64_t> lost)
{
    std::vector<bbr::AckedPacket> acked_pkts;
    std::vector<bbr::LostPacket> lost_pkts;
    for(auto seq_no : ack) {
        auto size = sent_pkts_.find(seq_no)->second;
        acked_pkts.push_back({seq_no, size, clock_});
        bytes_in_flight_ -= size;
    }
    for(auto seq_no : lost) {
        auto size = sent_pkts_.find(seq_no)->second;
        lost_pkts.push_back({seq_no, size});
        bytes_in_flight_ -= size;
    }
    auto sample = sampler_.on_congestion_event(clock_, acked_pkts, lost_pkts,
            max_bw_, bw_upper_bound_, round_count_);
    max_bw_ = std::max(max_bw_, sample.sample_max_bandwidth);

    return sample;
}

// Sends one packet and acks it.  Then, send 20 packets.  Finally, send
// another 20 packets while acknowledging previous 20.
void BandwidthSamplerTest::send_40pkts_and_ack_first20pkts(bbr::time::TimeDelta dt)
{
    // Send 20 packets at a constant inter-packet time.
    for (int i = 1; i <= 20; i++) {
        send_pkt(i);
        clock_ += dt;
    }

    // Ack packets 1 to 20, while sending new packets at the same rate as before.
    for (int i = 1; i <= 20; i++) {
        ack_pkt(i);
        send_pkt(i + 20);
        clock_ += dt;
    }
}

TEST_F(BandwidthSamplerTest, SendAndWait)
{
    auto pkt_time_inter = 10_ms;
    auto expected_bw = kRegularPktSize / pkt_time_inter;
    // Send packets at the constant bandwidth.
    for (int i = 1; i < 20; i++) {
        send_pkt(i);
        clock_ += pkt_time_inter;
        auto cur_bw = ack_pkt(i);
        EXPECT_EQ(cur_bw, expected_bw);
    }

    // Send packets at the exponentially decreasing bandwidth.
    for (int i = 20; i < 25; i++) {
        pkt_time_inter = pkt_time_inter * 2;
        expected_bw = expected_bw * 0.5;

        send_pkt(i);
        clock_ += pkt_time_inter;
        auto cur_bw = ack_pkt(i);
        EXPECT_EQ(cur_bw, expected_bw);
    }
}

TEST_F(BandwidthSamplerTest, SendPaced)
{
    auto pkt_time_inter = 1_ms;
    BandWidth expected_bw = kRegularPktSize / pkt_time_inter;

    send_40pkts_and_ack_first20pkts(pkt_time_inter);

    // Ack the packets 21 to 40, arriving at the correct bandwidth.
    BandWidth last_bandwidth = 0_bps;
    for (int i = 21; i <= 40; i++) {
        last_bandwidth = ack_pkt(i);
        EXPECT_EQ(expected_bw, last_bandwidth) << "i is " << i;
        clock_ += pkt_time_inter;
    }
    sampler_.remove_obsolete_pkts(41);
    EXPECT_EQ(0u, bytes_in_flight_);
}

TEST_F(BandwidthSamplerTest, SendWithLosses)
{
    auto pkt_time_inter = 1_ms;
    BandWidth expected_bw = kRegularPktSize / pkt_time_inter * 0.5;

    // Send 20 packets, each 1 ms apart.
    for (int i = 1; i <= 20; i++) {
        send_pkt(i);
        clock_ += pkt_time_inter;
    }

    // Ack packets 1 to 20, losing every even-numbered packet, while sending new
    // packets at the same rate as before.
    for (int i = 1; i <= 20; i++) {
        if (i % 2 == 0) {
            ack_pkt(i);
        } else {
            lose_pkt(i);
        }
        send_pkt(i + 20);
        clock_ += pkt_time_inter;
    }

    for (int i = 21; i <= 40; i++)
    {
        if (i % 2 == 0) {
            auto cur_bw = ack_pkt(i);
            EXPECT_EQ(expected_bw, cur_bw);
        } else {
            lose_pkt(i);
        }
        clock_ += pkt_time_inter;
    }
    EXPECT_EQ(0u, bytes_in_flight_);
}

TEST_F(BandwidthSamplerTest, NotCongestionControlled)
{
    auto pkt_time_inter = 1_ms;
    BandWidth expected_bw = kRegularPktSize / pkt_time_inter * 0.5;

    // Send 20 packets, each 1 ms apart. Every even packet is not congestion
    // controlled.
    for (int i = 1; i <= 20; i++) {
        send_pkt_inner(i, kRegularPktSize, i % 2 == 0);
        clock_ += pkt_time_inter;
    }

    // Ack packets 2 to 21, ignoring every even-numbered packet, while sending new
    // packets at the same rate as before.
    for (int i = 1; i <= 20; i++) {
        if (i % 2 == 0) {
            ack_pkt(i);
        }
        send_pkt_inner(i + 20, kRegularPktSize, i % 2);
        clock_ += pkt_time_inter;
    }

    // Ack the packets 22 to 41 with the same congestion controlled pattern.
    for (int i = 21; i <= 40; i++) {
        if (i % 2 == 0) {
            auto last_bandwidth = ack_pkt(i);
            EXPECT_EQ(expected_bw, last_bandwidth);
        }
        clock_ += pkt_time_inter;
    }
    sampler_.remove_obsolete_pkts(41);

    EXPECT_EQ(0u, bytes_in_flight_);
}

// Simulate a situation where ACKs arrive in burst and earlier than usual, thus
// producing an ACK rate which is higher than the original send rate.
TEST_F(BandwidthSamplerTest, CompressedAck)
{
    auto pkt_time_inter = 1_ms;
    BandWidth expected_bw = kRegularPktSize / pkt_time_inter;

    send_40pkts_and_ack_first20pkts(pkt_time_inter);

    // Simulate an RTT somewhat lower than the one for 1-to-21 transmission.
    clock_ += pkt_time_inter * 15;

    // Ack the packets 21 to 40 almost immediately at once.
    auto ridiculously_small_time_delta = 20_us;
    auto last_bandwidth = 0_mbps;
    for (int i = 21; i <= 40; i++) {
        last_bandwidth = ack_pkt(i);
        clock_ += ridiculously_small_time_delta;
    }

    EXPECT_EQ(expected_bw, last_bandwidth);
    EXPECT_EQ(0u, bytes_in_flight_);
}

TEST_F(BandwidthSamplerTest, ReorderedAck)
{
    auto pkt_time_inter = 1_ms;
    BandWidth expected_bw = kRegularPktSize / pkt_time_inter;

    send_40pkts_and_ack_first20pkts(pkt_time_inter);

    // Ack the packets 21 to 40 in the reverse order, while sending packets 41 to
    // 60.
    auto last_bandwidth = 0_bps;
    for (int i = 0; i < 20; i++) {
        last_bandwidth = ack_pkt(40 - i);
        EXPECT_EQ(expected_bw, last_bandwidth);
        send_pkt(41 + i);
        clock_ += pkt_time_inter;
    }

    // Ack the packets 41 to 60, now in the regular order.
    for (int i = 41; i <= 60; i++) {
        last_bandwidth = ack_pkt(i);
        EXPECT_EQ(expected_bw, last_bandwidth) << "i:" << i;
        clock_ += pkt_time_inter;
    }
    sampler_.remove_obsolete_pkts(61);
    EXPECT_EQ(0u, bytes_in_flight_);
}

// Test the app-limited logic.
TEST_F(BandwidthSamplerTest, AppLimited)
{
    auto pkt_time_inter = 1_ms;
    BandWidth expected_bw = kRegularPktSize / pkt_time_inter;

    // Send 20 packets at a constant inter-packet time.
    for (int i = 1; i <= 20; i++) {
        send_pkt(i);
        clock_ += pkt_time_inter;
    }

    // Ack packets 1 to 20, while sending new packets at the same rate as
    // before.
    for (int i = 1; i <= 20; i++) {
        BandwidthSample sample = ack_pkt_inner(i);
        EXPECT_EQ(sample.state_at_send.is_app_limited,
                app_limited_at_start_);
        send_pkt(i + 20);
        clock_ += pkt_time_inter;
    }

    // We are now app-limited. Ack 21 to 40 as usual, but do not send anything for
    // now.
    sampler_.on_app_limited();
    for (int i = 21; i <= 40; i++) {
        BandwidthSample sample = ack_pkt_inner(i);
        EXPECT_FALSE(sample.state_at_send.is_app_limited);
        EXPECT_EQ(expected_bw, sample.bandwidth);
        clock_ += pkt_time_inter;
    }

    // Enter quiescence.
    clock_ += 1_sec;

    // Send packets 41 to 60, all of which would be marked as app-limited.
    for (int i = 41; i <= 60; i++) {
        send_pkt(i);
        clock_ += pkt_time_inter;
    }

    // Ack packets 41 to 60, while sending packets 61 to 80.  41 to 60 should be
    // app-limited and underestimate the bandwidth due to that.
    for (int i = 41; i <= 60; i++) {
        BandwidthSample sample = ack_pkt_inner(i);
        EXPECT_TRUE(sample.state_at_send.is_app_limited);
        EXPECT_LT(sample.bandwidth, 0.7f * expected_bw );

        send_pkt(i + 20);
        clock_ += pkt_time_inter;
    }
    // Run out of packets, and then ack packet 61 to 80, all of which should have
    // correct non-app-limited samples.
    for (int i = 61; i <= 80; i++) {
        BandwidthSample sample = ack_pkt_inner(i);
        EXPECT_FALSE(sample.state_at_send.is_app_limited);
        EXPECT_EQ(sample.bandwidth, expected_bw);
        clock_ += pkt_time_inter;
    }
    sampler_.remove_obsolete_pkts(81);
    EXPECT_EQ(0u, bytes_in_flight_);
}

// Test the samples taken at the first flight of packets sent.
TEST_F(BandwidthSamplerTest, FirstRoundTrip)
{
    auto pkt_time_inter = 1_ms;
    const auto rtt = 800_ms;
    const int num_packets = 10;
    const size_t num_bytes = kRegularPktSize * num_packets;
    BandWidth real_bw = num_bytes / rtt;

    for (int i = 1; i <= 10; i++) {
        send_pkt(i);
        clock_ += pkt_time_inter;
    }

    clock_ += rtt - num_packets * pkt_time_inter;

    auto last_sample = 0_mbps;
    for (int i = 1; i <= 10; i++) {
        auto sample = ack_pkt(i);
        EXPECT_GT(sample, last_sample);
        last_sample = sample;
        clock_ += pkt_time_inter;
    }

    // The final measured sample for the first flight of sample is expected to be
    // smaller than the real bandwidth, yet it should not lose more than 10%. The
    // specific value of the error depends on the difference between the RTT and
    // the time it takes to exhaust the congestion window (i.e. in the limit when
    // all packets are sent simultaneously, last sample would indicate the real
    // bandwidth).
    EXPECT_LT(last_sample, real_bw);
    EXPECT_GT(last_sample, 0.9f * real_bw) << "v1:"<<last_sample.value() <<
            ",v2:" <<real_bw.value();
}

TEST_F(BandwidthSamplerTest, CongestionEventSampleDefaultValues) {
    // Make sure a default constructed CongestionEventSample has the correct
    // initial values for BandwidthSampler::OnCongestionEvent() to work.
    bbr::CongestionEventSample sample;

    EXPECT_FALSE(sample.sample_max_bandwidth.is_valid());
    EXPECT_FALSE(sample.sample_is_app_limited);
    EXPECT_FALSE(sample.sample_rtt.is_valid());
    EXPECT_EQ(0u, sample.sample_max_inflight);
    EXPECT_EQ(0u, sample.extra_acked);
}

TEST_F(BandwidthSamplerTest, TwoAckedPacketsPerEvent)
{
    auto time_between_packets = 10_ms;
    auto sending_rate = kRegularPktSize / time_between_packets;

    for (uint64_t i = 1; i < 21; i++) {
        send_pkt(i);
        clock_ += time_between_packets;
        if (i % 2 != 0) {
            continue;
        }

        auto sample = on_congestion_event({i - 1, i}, {});
        EXPECT_EQ(sending_rate, sample.sample_max_bandwidth);
        EXPECT_EQ(time_between_packets, sample.sample_rtt);
        EXPECT_EQ(2 * kRegularPktSize, sample.sample_max_inflight);
        EXPECT_TRUE(sample.last_packet_send_state.is_valid);
        EXPECT_EQ(2 * kRegularPktSize, sample.last_packet_send_state.bytes_in_flight);
        EXPECT_EQ(i * kRegularPktSize, sample.last_packet_send_state.total_bytes_sent);
        EXPECT_EQ((i - 2) * kRegularPktSize, sample.last_packet_send_state.total_bytes_acked);
        EXPECT_EQ(0u, sample.last_packet_send_state.total_bytes_lost);
        sampler_.remove_obsolete_pkts(i - 2);
    }
}

TEST_F(BandwidthSamplerTest, LoseEveryOtherPacket)
{
    auto time_between_packets = 10_ms;
    auto sending_rate = kRegularPktSize / time_between_packets;

    for (uint64_t i = 1; i < 21; i++) {
        send_pkt(i);
        clock_ += time_between_packets;
        if (i % 2 != 0) {
            continue;
        }

        // Ack packet i and lose i-1.
        auto sample = on_congestion_event({i}, {i - 1});
        // Losing 50% packets means sending rate is twice the bandwidth.
        EXPECT_EQ(sending_rate, sample.sample_max_bandwidth * 2);
        EXPECT_EQ(time_between_packets, sample.sample_rtt);
        EXPECT_EQ(kRegularPktSize, sample.sample_max_inflight);
        EXPECT_TRUE(sample.last_packet_send_state.is_valid);
        EXPECT_EQ(2 * kRegularPktSize,
              sample.last_packet_send_state.bytes_in_flight);
        EXPECT_EQ(i * kRegularPktSize,
              sample.last_packet_send_state.total_bytes_sent);
        EXPECT_EQ((i - 2) * kRegularPktSize / 2,
              sample.last_packet_send_state.total_bytes_acked);
        EXPECT_EQ((i - 2) * kRegularPktSize / 2,
              sample.last_packet_send_state.total_bytes_lost);
        sampler_.remove_obsolete_pkts(i - 2);
    }
}

TEST_F(BandwidthSamplerTest, AckHeightRespectBandwidthEstimateUpperBound)
{
    auto time_between_packets = 10_ms;
    auto first_packet_sending_rate = kRegularPktSize / time_between_packets;

    // Send and ack packet 1.
    send_pkt(1);
    clock_ += time_between_packets;
    auto sample = on_congestion_event({1}, {});
    EXPECT_EQ(first_packet_sending_rate, sample.sample_max_bandwidth);
    EXPECT_EQ(first_packet_sending_rate, max_bw_);

    // Send and ack packet 2, 3 and 4.
    round_count_++;
    bw_upper_bound_ = first_packet_sending_rate * 0.3;
    send_pkt(2);
    send_pkt(3);
    send_pkt(4);
    clock_ += time_between_packets;
    sample = on_congestion_event({2, 3, 4}, {});
    EXPECT_EQ(first_packet_sending_rate * 3, sample.sample_max_bandwidth);
    EXPECT_EQ(max_bw_, sample.sample_max_bandwidth);

    EXPECT_LT(2 * kRegularPktSize, sample.extra_acked);
}

class MaxAckHeightTrackerTest : public testing::Test {
public:
    MaxAckHeightTrackerTest()
        : tracker_(10)
    {
        tracker_.set_threshold(1.8);
    }

    void aggreation_episode(BandWidth aggregation_bw, TimeDelta aggregation_durataion,
            size_t bytes_per_ack, bool expect_new_aggregation_epoch)
    {
        ASSERT_GE(aggregation_bw , bw_);
        const auto start_time = now_;
        const size_t aggregation_bytes = aggregation_bw * aggregation_durataion / 8;

        const int num_acks = aggregation_bytes / bytes_per_ack;
        ASSERT_EQ(aggregation_bytes, num_acks * bytes_per_ack)
            << "aggregation_bytes: " << aggregation_bytes << "["
            <<aggregation_bw.to_str() <<" in" <<aggregation_durataion.to_str()
            <<"], bytes per ack:"<<bytes_per_ack;

        auto ack_interval = aggregation_durataion / num_acks;
        ASSERT_EQ(aggregation_durataion, num_acks * ack_interval)
            << "aggregation_bytes: " << aggregation_bytes
            << ", num_acks: " << num_acks
            << ", time_between_acks: " << ack_interval.to_str();

        auto total_duration = aggregation_bytes / bw_;
        ASSERT_EQ(aggregation_bytes * 8, bw_ * total_duration)
            << "total_duration: " << total_duration.to_str()
            << ", bandwidth_: " << bw_.to_str();

        size_t last_extra_acked = 0;
        for(size_t bytes = 0; bytes < aggregation_bytes; bytes += bytes_per_ack) {
            auto extra_acked = tracker_.update(bw_, round_count(), now_, bytes_per_ack);
//            std::cout<< "T" << now_.to_str() << ": Update after " << bytes_per_ack
//                     <<" bytes acked, " << extra_acked << " extra bytes acked";
            if((bytes == 0 && expect_new_aggregation_epoch) ||
                    (aggregation_bw == bw_)) {
                EXPECT_EQ(0, extra_acked);
            } else {
                EXPECT_LT(last_extra_acked, extra_acked);
            }
            now_ = now_ + ack_interval;
            last_extra_acked = extra_acked;
        }
        // Advance past the quiet period.
        now_ = start_time + total_duration;
    }

    int round_count() const {
      return (now_ - Timestamp(0)) / rtt_;
    }
protected:
    bbr::MaxAckHeightTracker tracker_;
    bbr::time::TimeDelta rtt_ = 60_ms;
    BandWidth bw_ = 10 * 1000 / 1_sec;
    bbr::time::Timestamp now_ {1000};
};

TEST_F(MaxAckHeightTrackerTest, VeryAggregatedLargeAck)
{
    aggreation_episode(bw_ * 20, 6_ms, 1200, true);
    aggreation_episode(bw_ * 20, 6_ms, 1200, true);
    now_ = now_ - 1_ms;

    if (tracker_.threshold() > 1.1) {
        aggreation_episode(bw_ * 20, 6_ms, 1200, true);
        EXPECT_EQ(3u, tracker_.num_ack_aggregation_epochs());
    } else {
        aggreation_episode(bw_ * 20, 6_ms, 1200, false);
        EXPECT_EQ(2u, tracker_.num_ack_aggregation_epochs());
    }
}

TEST_F(MaxAckHeightTrackerTest, VeryAggregatedSmallAcks)
{
    aggreation_episode(bw_ * 20, 6_ms, 300, true);
    aggreation_episode(bw_ * 20, 6_ms, 300, true);
    now_ = now_ - 1_ms;

    if (tracker_.threshold() > 1.1) {
        aggreation_episode(bw_ * 20, 6_ms, 300, true);
        EXPECT_EQ(3u, tracker_.num_ack_aggregation_epochs());
    } else {
        aggreation_episode(bw_ * 20, 6_ms, 300, true);
        EXPECT_EQ(2u, tracker_.num_ack_aggregation_epochs());
    }
}

TEST_F(MaxAckHeightTrackerTest, SomewhatAggregatedLargeAck)
{
    aggreation_episode(bw_ * 2, 50_ms, 1000, true);
    aggreation_episode(bw_ * 2, 50_ms, 1000, true);
    now_ = now_ - 1_ms;

    if (tracker_.threshold() > 1.1) {
        aggreation_episode(bw_ * 2, 50_ms, 1000, true);
        EXPECT_EQ(3u, tracker_.num_ack_aggregation_epochs());
    } else {
        aggreation_episode(bw_ * 2, 50_ms, 1000, false);
        EXPECT_EQ(2u, tracker_.num_ack_aggregation_epochs());
    }
}

TEST_F(MaxAckHeightTrackerTest, SomewhatAggregatedSmallAcks)
{
    aggreation_episode(bw_ * 2, 50_ms, 100, true);
    aggreation_episode(bw_ * 2, 50_ms, 100, true);
    now_ = now_ - 1_ms;

    if (tracker_.threshold() > 1.1) {
        aggreation_episode(bw_ * 2, 50_ms, 100, true);
        EXPECT_EQ(3u, tracker_.num_ack_aggregation_epochs());
    } else {
        aggreation_episode(bw_ * 2, 50_ms, 100, false);
        EXPECT_EQ(2u, tracker_.num_ack_aggregation_epochs());
    }
}

TEST_F(MaxAckHeightTrackerTest, NotAggregated)
{
    aggreation_episode(bw_, 100_ms, 100, true);
    EXPECT_LT(2u, tracker_.num_ack_aggregation_epochs());
}
