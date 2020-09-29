#include <bbr_sender.h>
#include <cstring>
#include <time/timestamp.h>

namespace bbr
{

BbrSender::BbrSender(PacketSender* sender)
    :socket_(sender)
{
    ;
}

bool BbrSender::send_or_queued_pkt(uint64_t pkt_seq_no,
        const uint8_t* data, size_t size)
{
    if(!bbr_.can_send(bytes_inflight())) {
        PacketBuffer::Packet cahced_pkt{pkt_seq_no, {}};
        cahced_pkt.data.resize(size);
        memcpy(cahced_pkt.data.data(), data, size);
        pkts_buffer_.insert(std::move(cahced_pkt));
        return false;
    }

    auto now = time::Timestamp::now();
    socket_->send_pkt(pkt_seq_no, data, size);
    bbr_.on_packet_sent(pkt_seq_no, size, bytes_inflight(),
            true, now);

    return true;
}
}
