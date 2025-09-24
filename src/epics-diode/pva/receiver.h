/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_PVA_RECEIVER_H
#define EPICS_DIODE_PVA_RECEIVER_H

#include <memory>
#include <string>

#include <epics-diode/config.h>

#include <pvxs/data.h>

namespace epics_diode {
namespace pva {

class Receiver {
public:
    using Callback = std::function<void(uint32_t channel_index, const ::pvxs::Value& value)>;

    Receiver(const epics_diode::Config& config, int port, std::string listening_address);
    ~Receiver();
    void run(double runtime, Callback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}
}

#endif
