/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_TRANSPORT_H
#define EPICS_DIODE_TRANSPORT_H

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include <osiSock.h>
#include <epicsStdlib.h>
#include <epicsString.h>

#include <epics-diode/logger.h>

namespace epics_diode {

constexpr int EPICS_DIODE_DEFAULT_PORT = 5080;
constexpr int EPICS_PVADIODE_DEFAULT_PORT = 5081;

const std::string EPICS_DIODE_DEFAULT_LISTENING_ADDRESS = "0.0.0.0";


struct SocketContext {
    SocketContext() {
        osiSockAttach();
    }
    
    ~SocketContext() { 
        osiSockRelease();
    }
};

std::vector<osiSockAddr> parse_socket_address_list(const std::string& list, int default_port);

std::string to_string(const osiSockAddr& addr);


class UDPSender {
public:
    UDPSender(std::vector<osiSockAddr> send_addresses, uint32_t rate_limit_mbs);
    ~UDPSender();

    void send(const uint8_t* buffer, std::size_t length);

private:
    Logger logger;
    
    const uint32_t rate_limit_mbs;
    SOCKET socket;
    std::vector<osiSockAddr> send_addresses;

    using clock_type = std::chrono::steady_clock;

    std::size_t last_sent_bytes = 0;
    std::chrono::time_point<clock_type> last_sent_time;

    static constexpr std::size_t MIN_RATE_REPORT_PERIOD_US = 3000000;      // 3s
    std::size_t last_report_sent_bytes = 0;
    std::size_t last_report_period_us = 0;
};


class UDPReceiver {
public:
    explicit UDPReceiver(int port, std::string listening_address);
    ~UDPReceiver();

    ssize_t receive(const uint8_t* buffer, std::size_t length, osiSockAddr* fromAddress);

private:
    Logger logger;
    SOCKET socket;
};

}

#endif

