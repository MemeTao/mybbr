#ifndef BBR_BBR_ALGORITHM_H_
#define BBR_BBR_ALGORITHM_H_

#include <cstddef>
#include <cstdint>
#include <bbr_model.h>

namespace bbr
{


class BbrAlgorithm
{
public:
    Bbrparams& params() {
        return params_;
    }
    size_t min_cwnd() const;
private:
    Bbrparams params_;
};
}
#endif
