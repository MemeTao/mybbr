#ifndef BBR_ROUDN_TRIP_COUNTER_H_
#define BBR_ROUDN_TRIP_COUNTER_H_

#include <cstddef>
#include <cstdint>

namespace bbr
{
class RoundTripCounter
{
public:
    void on_pkt_sent(uint64_t seq_no) {
        last_sent_packet_no_ = seq_no;
    }
    // Return whether a round trip has just completed.
    bool on_pkt_acked(uint64_t acked_seq_no) {
        if (acked_seq_no > end_of_round_trip_) {
            round_trip_count_++;
            end_of_round_trip_ = last_sent_packet_no_;
            return true;
        }
        return false;
    }

    uint64_t count() {
        return round_trip_count_;
    }

private:
    uint64_t round_trip_count_ = 0;
    uint64_t last_sent_packet_no_ = std::numeric_limits<uint64_t>::max();
    // The last sent packet number of the current round trip.
    uint64_t end_of_round_trip_ = 0;
};
}
#endif
