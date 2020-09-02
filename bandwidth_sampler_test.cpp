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
        :sampler_(0)
    {}
    void send_pkt(uint64_t seq_no);
    BandWidth ack_pkt(uint64_t seq_no);
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
}
