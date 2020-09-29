#include <loss_detect.h>
#include <packet_history.h>

namespace bbr
{
std::vector<uint64_t> LossDetect::on_pkts_ack(
        AckedPkt left, AckedPkt right,
        const std::vector<AckTrunk>& blocks)
{
    std::vector<uint64_t> lost;

    return lost;
}
}
