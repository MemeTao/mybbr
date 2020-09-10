#ifndef BBR_MODE_H_
#define BBR_MODE_H_

#include <cstddef>
#include <cstdint>

namespace bbr
{
enum class BbrMode : uint8_t {
    // Startup phase of the connection.
    STARTUP,
    // After achieving the highest possible bandwidth during the startup, lower
    // the pacing rate in order to drain the queue.
    DRAIN,
    // Cruising mode.
    PROBE_BW,
    // Temporarily slow down sending in order to empty the buffer and measure
    // the real minimum RTT.
    PROBE_RTT,
};
}

#endif
