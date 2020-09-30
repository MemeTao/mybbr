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
    //TODO: increase bytes_inflight

    return ret;
}

void BbrSender::on_pkt_ack(const AckedPacket& pkt)
{
    auto now = time::Timestamp::now();
    auto lost_nos = loss_detect_.on_pkt_ack(pkt);

    std::vector<internal::LostPacket> lost_pkts;
    for(auto lost_no : lost_nos) {
        auto lost = pkts_history_.find(lost_no);
        if(lost == nullptr) {
            //log error
            continue;
        }
        lost_pkts.push_back({lost->seq_no, lost->size});
    }

    auto sending_pkt = pkts_history_.find(pkt.seq_no);
    if(!sending_pkt || sending_pkt->seq_no != pkt.seq_no) {
        //log error
        return ;
    }
    internal::AckedPacket acked_pkt{pkt.seq_no, sending_pkt->size, pkt.arrival_time};
    bbr_.on_congestion_event(bytes_inflight(), now, {acked_pkt}, lost_pkts);

    check_after_acked();
    //TODO: erase thoes pkts on 'packethisotry' that will not be used anymore
}


void BbrSender::on_pkts_ack(const std::vector<AckedTrunk>& trunks)
{
    auto now = time::Timestamp::now();
    auto lost_nos = loss_detect_.on_pkts_ack(trunks);

    std::vector<internal::LostPacket> lost_pkts;
    for(auto lost_no : lost_nos) {
        auto lost = pkts_history_.find(lost_no);
        if(lost == nullptr) {
            //log error
            assert(false);
            continue;
        }
        lost_pkts.push_back({lost->seq_no, lost->size});
    }
    std::vector<internal::AckedPacket> acked_pkts;
    for(const auto& trunk : trunks) {
        assert(trunk.seq_no_end >= trunk.seq_no_begin);
        assert(trunk.arrival_times.size() ==
                trunk.seq_no_end-trunk.seq_no_begin+1);
        for(uint64_t seq_no = trunk.seq_no_begin;
                seq_no <= trunk.seq_no_end; seq_no ++)
        {
            auto acked = pkts_history_.find(seq_no);
            if(acked == nullptr) {
                //log error
                assert(false);
                continue;
            }
            acked_pkts.push_back({acked->seq_no, acked->size,
                trunk.arrival_times[seq_no-trunk.seq_no_begin]});
        }
    }
    bbr_.on_congestion_event(bytes_inflight(), now, acked_pkts, lost_pkts);

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
