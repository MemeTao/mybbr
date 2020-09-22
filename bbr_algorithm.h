#ifndef BBR_BBR_ALGORITHM_H_
#define BBR_BBR_ALGORITHM_H_

#include <cstddef>
#include <cstdint>
#include <bbr_model.h>
#include <common/rate.h>
#include <common/random.h>
#include <bbr_startup.h>
#include <bbr_drain.h>
#include <bbr_probe_bw.h>

namespace bbr
{

class BbrAlgorithm
{
public:
    Bbrparams& params()
    {
        return params_;
    }

    size_t min_cwnd() const; //limited on params

    size_t cwnd();

    common::Random& random()
    {
        return random_;
    }

    size_t target_inflight() const;
private:
    Bbrparams params_;
    common::Random random_;

    const size_t init_cwnd_;

    size_t cur_cwnd_;
    common::BitRate pacing_rate_;

    BbrModel model_;

    BbrStartupMode mode_start_up_;
    BbrDrainMode mode_bbr_drain_;
    BbrProbeBandwidth mode_probe_bw_;

};
}
#endif
