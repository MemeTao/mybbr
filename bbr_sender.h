#ifndef BBR_SENDER_H_
#define BBR_SENDER_H_

#include <cstddef>
#include <cstdint>
#include <packet_buffer.h>
#include <bbr_algorithm.h>

namespace bbr
{

class PacketSender {
public:
    void send_pkt(const SendingPacket& pkt) = 0;
};

class BbrSender
{
public:

    BbrSender(PacketSender* sender);
    // return false if bbr determines to buffered this pkt
    // packet size must be less than 1460
    bool send_or_queued_pkt(const SendingPacket& pkt);

    void on_pkt_ack(const AckedPacket& pkt);

private:
    size_t bytes_inflight() const;

    BbrAlgorithm bbr_;
    PacketBuffer pkts_buffer_;

    PacketSender* socket_;
};
}
#endif
