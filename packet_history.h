#ifndef BBR_PACKET_HISTORY_H_
#define BBR_PACKET_HISTORY_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <map>
#include <time/timestamp.h>

namespace bbr
{

template <typename PacketType,
    template<typename> class ContainerType = std::map>
class PacketHistory
{
public:
    struct SentPkt {
        bool valid = false;
        uint64_t seq_no = 0;
        size_t bytes = 0;
        time::Timestamp sent_time;
    };

    void insert(const PacketType& pkt);

    const PacketType& find(uint64_t seq_no) const;

    size_t erase(uint64_t seq_no);

    size_t erase(uint64_t begin, uint64_t end);

private:
    ContainerType<PacketType> buffer_;
};
}
#endif
