#ifndef BBR_BBR_ALGORITHM_H_
#define BBR_BBR_ALGORITHM_H_

#include <cstddef>
#include <cstdint>
#include <bbr_model.h>
#include <common/random.h>

namespace bbr
{

class BbrAlgorithm
{
public:
    Bbrparams& params() {
        return params_;
    }

    size_t min_cwnd() const; //limited on params

    size_t cwnd();

    common::Random& random() {
        return random_;
    }

    size_t target_inflight() const;
private:
    Bbrparams params_;
    common::Random random_;
};
}
#endif
