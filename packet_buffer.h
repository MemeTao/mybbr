#ifndef BBR_PACKET_BUFFER_H_
#define BBR_PACKET_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <vector>
#include <bbr.h>

namespace bbr
{

class PacketBuffer
{
public:
    struct Packet {
        bool valid = false;
        SendingPacket pkt;
    };
    void insert(Packet&& pkt);

    Packet get(uint64_t seq_no);

    Packet pop();

};
}
#endif
