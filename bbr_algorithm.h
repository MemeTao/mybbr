#ifndef BBR_BBR_ALGORITHM_H_
#define BBR_BBR_ALGORITHM_H_

#include <cstddef>
#include <cstdint>
#include <bbr_model.h>
#include <common/rate.h>
#include <common/random.h>
#include <bbr_mode.h>
#include <bbr_startup.h>
#include <bbr_drain.h>
#include <bbr_probe_bw.h>
#include <bbr_probe_rtt.h>

namespace bbr
{

class BbrAlgorithm
{
public:
    void on_packet_sent(uint64_t pkt_no,
            size_t bytes, size_t bytes_in_flight,
            bool need_retransmitted,
            time::Timestamp sent_time);

    void on_congestion_event(
        size_t prior_inflight,
        time::Timestamp at_time,
        const std::vector<AckedPacket>& acked_packets,
        const std::vector<LostPacket>& lost_packets);

    size_t min_cwnd() const {return params_.min_cwnd;}

    size_t cwnd() const { return cur_cwnd_;}

    size_t target_cwnd(float gain);

    common::Random& random() { return random_; }

    Bbrparams& params() { return params_;}

    size_t target_inflight() const;

private:
    void update_cwnd(size_t bytes_acked);
    void update_pacing_rate(size_t bytes_acked);

    size_t cwnd_upper_limit();

    void on_exit_quiescence(time::Timestamp at_time);

private:
    Bbrparams params_;
    common::Random random_;

    const size_t init_cwnd_;
    size_t cur_cwnd_;
    common::BitRate pacing_rate_;

    BbrModel model_;

    BbrMode cur_mode_;

    BbrStartupMode mode_start_up_;
    BbrDrainMode mode_drain_;
    BbrProbeBandwidth mode_probe_bw_;
    BbrProbeRtt mode_probe_rtt_;

    time::Timestamp last_quiescence_start_;
};
}
#endif
