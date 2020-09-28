#ifndef BBR_PROBE_BW_H_
#define BBR_PROBE_BW_H_

#include <vector>
#include <common/rate.h>
#include <time/timestamp.h>
#include <bbr_common.h>
#include <bbr_mode.h>

namespace bbr
{
class BbrAlgorithm;
class BbrModel;
struct BbrCongestionEvent;

class BbrProbeBandwidth
{
    enum class CyclePhase : uint8_t {
        kProbeNotStarted = 0,
        kPorbeUp,
        kPorbeDown,
        kPorbeCruise,
        kProbeRefill,
    };

    struct Cycle {
        time::Timestamp cycle_start_time{0};
        CyclePhase phase = CyclePhase::kProbeNotStarted;
        uint64_t rounds_in_phase = 0;
        time::Timestamp phase_start_time {0};
        uint64_t rounds_since_probe = 0;
        time::TimeDelta probe_wait_time{0};
        uint64_t probe_up_rounds = 0;
        size_t probe_up_bytes = std::numeric_limits<size_t>::max();
        size_t probe_up_acked = 0;
        // Whether max bandwidth filter window has advanced in this cycle.
        // It is advanced once per cycle.
        bool has_advanced_max_bw = false;
        bool is_sample_from_probing = false;  /* rate samples reflect bw probing? */
    };

    enum class AdaptUpperBoundsResult : uint8_t {
        kOk,
        kProbedTooHigh,
        kInfilghtHighNotSet,
        kInvalidSample
    };

public:
    BbrProbeBandwidth(BbrAlgorithm* bbr,  BbrModel* model);

    void enter(time::Timestamp now,
               const BbrCongestionEvent* congestion_event);

    void leave(time::Timestamp now,
               const BbrCongestionEvent* congestion_event) {};

    BbrMode on_congestion_event(
        size_t prior_inflight,
        time::Timestamp at_time,
        const std::vector<AckedPacket>& acked_packets,
        const std::vector<LostPacket>& lost_packets,
        const BbrCongestionEvent& congestion_event);

    BbrMode on_exit_quiescence(time::Timestamp quiescence_start_time,
            time::Timestamp now);

    bool is_probing() const {
        return cycle_.phase == CyclePhase::kProbeRefill ||
                cycle_.phase == CyclePhase::kPorbeUp;
    }

    size_t cwnd_upper_limit() const ;

private:
    float pacing_gain(CyclePhase phase) const;

    void enter_probe_down(bool probed_too_high,
            bool stopped_risky_probe, time::Timestamp now);
    void exist_probe_down();
    void enter_probe_up(time::Timestamp now);
    void enter_probe_cruise(time::Timestamp now);
    void enter_probe_refill(uint64_t probe_up_rounds,
            time::Timestamp now);

    void update_probe_down(size_t prior_inflight,
            const BbrCongestionEvent& congestion_event);
    void update_probe_up(size_t prior_inflight,
            const BbrCongestionEvent& congestion_event);
    void update_probe_cruise(
            const BbrCongestionEvent& congestion_event);
    void update_probe_refill(
            const BbrCongestionEvent& congestion_event);
private:
    bool is_time_to_probe_bw(
            const BbrCongestionEvent& congestion_event);
    bool is_time_to_probe_for_reno_coexistence(
            double probe_wait_fraction,
            const BbrCongestionEvent& congestion_event);

    bool has_stayed_long_enough_in_probe_down(
            const BbrCongestionEvent& congestion_event);

    void handle_inflight_too_high(
            bool app_limited, size_t bytes_inflight);

    AdaptUpperBoundsResult maybe_adapt_upper_bounds(
            const BbrCongestionEvent& congestion_event);

    void raise_inflight_hi();
    void probe_inflight_high_upward(
            const BbrCongestionEvent& congestion_event);
private:
    BbrAlgorithm* bbr_;
    BbrModel* model_;

    Cycle cycle_;

    bool last_cycle_probed_too_high_ = false;
    bool last_cycle_stopped_risky_probe_ = false;
};
}
#endif
