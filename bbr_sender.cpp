#include <bbr_sender.h>
#include <cassert>
#include <cstring>
#include <time/timestamp.h>

namespace bbr
{

BbrSender::BbrSender(PacketSender* sender)
    :socket_(sender)
{
    assert(socket_ != nullptr);
}

bool BbrSender::send_or_queued_pkt(SendingPacket&& pkt)
{
    size_t bytes_inflight = bytes_inflight();
    if(!bbr_.can_send(bytes_inflight)) {
        pkts_buffer_.insert(PacketBuffer::Packet{false, std::move(pkt)});
        return false;
    }
    return send_pkt(std::move(pkt));
}

bool BbrSender::send_pkt(SendingPacket&& pkt)
{
    auto now = time::Timestamp::now();
    bool ret = socket_->send_pkt(std::move(pkt));
    bbr_.on_packet_sent(pkt.seq_no, pkt.size, bytes_inflight(),
            true, now);
    pkts_history_.insert(pkt.seq_no, SentPktType{pkt, now});

    bytes_inflight_ += pkt.size;

    return ret;
}

void BbrSender::on_pkt_ack(const AckedPacket& pkt)
{
    auto now = time::Timestamp::now();
    size_t prior_bytes_infligth = bytes_inflight();
    auto lost_nos = loss_detect_.on_pkt_ack(pkt);

    size_t lost_bytes = 0;
    std::vector<internal::LostPacket> lost_pkts;
    for(auto lost_no : lost_nos) {
        auto lost = pkts_history_.find(lost_no);
        assert(lost != nullptr);
        lost_pkts.push_back({lost->pkt.seq_no, lost->pkt.size});
        lost_bytes += lost->pkt.size;
    }
    assert(bytes_inflight_ >= lost_bytes);
    bytes_inflight_ -= lost_bytes;

    auto sending_pkt = pkts_history_.find(pkt.seq_no);
    if(!sending_pkt || sending_pkt->pkt.seq_no != pkt.seq_no) {
        //log error
        return ;
    }
    assert(bytes_inflight_ >= sending_pkt->pkt.size);
    bytes_inflight_ -= sending_pkt->pkt.size;

    internal::AckedPacket acked_pkt{pkt.seq_no, sending_pkt->pkt.size, pkt.arrival_time};
    bbr_.on_congestion_event(prior_bytes_infligth, now, {acked_pkt}, lost_pkts);

    check_after_acked();
    //TODO: erase thoes pkts on 'packethisotry' that will not be used anymore
}

void BbrSender::on_pkts_ack(const std::vector<AckedTrunk>& trunks)
{
    auto now = time::Timestamp::now();
    size_t prior_bytes_infligth = bytes_inflight();
    auto lost_nos = loss_detect_.on_pkts_ack(trunks);

    size_t lost_bytes = 0;
    std::vector<internal::LostPacket> lost_pkts;
    for(auto lost_no : lost_nos) {
        auto lost = pkts_history_.find(lost_no);
        assert(lost != nullptr);
        lost_pkts.push_back({lost->pkt.seq_no, lost->pkt.size});
        lost_bytes += lost->pkt.size;
    }
    assert(bytes_inflight >= lost_bytes);
    bytes_inflight_ -= lost_bytes;

    size_t acked_bytes = 0;
    std::vector<internal::AckedPacket> acked_pkts;
    for(const auto& trunk : trunks) {
        assert(trunk.seq_no_end >= trunk.seq_no_begin);
        assert(trunk.arrival_times.size() ==
                trunk.seq_no_end-trunk.seq_no_begin+1);
        for(uint64_t seq_no = trunk.seq_no_begin;
                seq_no <= trunk.seq_no_end; seq_no ++)
        {
            auto acked = pkts_history_.find(seq_no);
            //if we received fake ack-frame, ignore it
            if(acked == nullptr) {
                continue;
            }
            acked_pkts.push_back({acked->pkt.seq_no, acked->pkt.size,
                trunk.arrival_times[seq_no-trunk.seq_no_begin]});
            acked_bytes += acked->pkt.size;
        }
    }
    assert(bytes_inflight >= acked_bytes);
    bytes_inflight_ -= acked_bytes;

    bbr_.on_congestion_event(prior_bytes_infligth, now, acked_pkts, lost_pkts);

    check_after_acked();
    //TODO: erase thoes pkts on 'packethisotry' that will not be used anymore
}

void BbrSender::check_after_acked()
{
    // 1) check if we can send buffered pkts now
    while(pkts_buffer_.size() && bbr_.can_send(bytes_inflight())) {
        auto pkt = pkts_buffer_.pop();
        send_pkt(std::move(pkt.pkt));
    }
    // 2) any else ?
}

}
