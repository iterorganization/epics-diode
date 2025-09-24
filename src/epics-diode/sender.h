/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_SENDER_H
#define EPICS_DIODE_SENDER_H

#include <memory>
#include <string>

#include <epics-diode/config.h>

namespace epics_diode {

class Sender {
public:
    Sender(const epics_diode::Config& config, const std::string& send_addresses);
    ~Sender();
    void run(double runtime);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}

#endif