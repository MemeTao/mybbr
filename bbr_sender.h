#ifndef BBR_SENDER_H_
#define BBR_SENDER_H_

#include <cstddef>
#include <cstdint>
#include <packet_history.h>
#include <packet_buffer.h>
#include <loss_detect.h>
#include <bbr_algorithm.h>

namespace bbr
{

class PacketSender {
public:
    bool send_pkt(SendingPacket&& pkt) = 0;
};

class BbrSender
{
public:
    BbrSender(PacketSender* sender);
    // return false if bbr determines to buffered this pkt
    // packet size must be less than 1460
    bool send_or_queued_pkt(SendingPacket&& pkt);

    //two packet acked callback methods
    //1) only ack one packet(perhaps most cases)
    //2) ack a range that contains multiple contiguous tunks
    void on_pkt_ack(const AckedPacket& pkt);

    void on_pkts_ack(const std::vector<AckedTrunk>& trunks);

    //recommanded bandwidth
    common::BandWidth bandwidth() const;

private:
    bool send_pkt(SendingPacket&& pkt);

private:
    void check_after_acked();
    size_t bytes_inflight() const;

    BbrAlgorithm bbr_;
    LossDetect loss_detect_;

    PacketHistory<SendingPacket> pkts_history_; //buffered all sent but not acked packets
    PacketBuffer pkts_buffer_; //buffered sending packets

    PacketSender* socket_;
};
}
#endif
