/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_RECEIVER_H
#define EPICS_DIODE_RECEIVER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <epics-diode/config.h>

namespace epics_diode {

class Receiver {
public:
    using Callback = std::function<void(uint32_t channel_index, uint16_t type, uint32_t count, void* value)>;

    Receiver(const epics_diode::Config& config, int port, std::string listening_address);
    ~Receiver();
    void run(double runtime, Callback callback);

    // For testing: get sequence number of packet currently being processed
    uint16_t get_current_seq_no() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}

#endif
