#ifndef BBR_PACKET_HISTORY_H_
#define BBR_PACKET_HISTORY_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <deque>
#include <time/timestamp.h>

namespace bbr
{
template <typename PacketType,
    template<typename Elem>
         class ContainerType>
class PacketHistory
{
public:
    struct SentPkt {
        PacketType pkt;
        time::Timestamp sent_time;
    };

    void insert(uint64_t seq_no, SentPkt&& pkt) {
        buffer_.emplace(seq_no, std::move(pkt));
    }

    SentPkt* find(uint64_t seq_no) {
        return buffer_.get(seq_no);
    }

    void erase(uint64_t seq_no) {
        //TODO:
        return;
    }

private:
    ContainerType<SentPkt> buffer_;
};

}
#endif
