#include <gtest/gtest.h>
#include <time/time_stamp.h>
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
    EXPECT_FALSE(sample.sample_max_bandwidth.is_valid());
    EXPECT_FALSE(sample.sample_rtt.is_valid());
    return sample.last_packet_send_state;
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

