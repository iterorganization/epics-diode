/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <osiSock.h>
#include <epicsStdlib.h>
#include <epicsString.h>

#include <epics-diode/logger.h>
#include <epics-diode/transport.h>

namespace epics_diode {

namespace {

std::string get_socket_error_string() 
{
    std::array<char, 64> errStr{};
    epicsSocketConvertErrnoToString(errStr.begin(), errStr.size());
    return std::string(errStr.begin());
}

}

std::string to_string(const osiSockAddr& addr)
{
    std::array<char, 64> strBuffer{};
    sockAddrToDottedIP(&addr.sa, strBuffer.begin(), strBuffer.size());
    return std::string(strBuffer.begin());
}

std::vector<osiSockAddr> parse_socket_address_list(const std::string& list, int default_port) {

    std::vector<osiSockAddr> addresses;

    // skip leading spaces
    std::size_t len = list.length();
    std::size_t sub_start = 0;
    while (sub_start < len && isspace(list[sub_start])) {
        sub_start++;
    }

    // parse string
    std::size_t sub_end;
    while ((sub_end = list.find(' ', sub_start)) != std::string::npos) {
        std::string address = list.substr(sub_start, (sub_end - sub_start));
        osiSockAddr addr;
        if (aToIPAddr(address.c_str(), default_port, &addr.ia) == 0) {
            addresses.emplace_back(addr);
        }
        sub_start = list.find_first_not_of(" \t\r\n\v", sub_end);
    }

    if (sub_start != std::string::npos && sub_start < len) {
        osiSockAddr addr;
        if (aToIPAddr(list.substr(sub_start).c_str(), default_port, &addr.ia) == 0) {
            addresses.emplace_back(addr);
        }
    }

    return addresses;
}

UDPSender::UDPSender(std::vector<osiSockAddr> send_addresses, uint32_t rate_limit_mbs) :
    logger("transport.sender"),
    rate_limit_mbs(rate_limit_mbs),
    socket(epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP)),
    send_addresses(std::move(send_addresses))
{
    if (socket == INVALID_SOCKET)
    {
        throw std::runtime_error(std::string("Failed to create a socket: ") +
            get_socket_error_string());
    }
}

UDPSender::~UDPSender() {
    if (socket != INVALID_SOCKET) {
        epicsSocketDestroy(socket);
    }
}

void UDPSender::send(const uint8_t* buffer, std::size_t length) {

    // do rate-limiting, if enabled
    if (rate_limit_mbs > 0) {
        std::chrono::microseconds calculated_period_us(last_sent_bytes / rate_limit_mbs);
        
        auto elapsed = clock_type::now() - last_sent_time;
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
        auto diff = calculated_period_us - elapsed_us;

        if (diff.count() > 0) {
            std::this_thread::sleep_for(diff);
        }

        last_report_sent_bytes += last_sent_bytes;
        last_report_period_us += elapsed_us.count();
        if (last_report_period_us >= MIN_RATE_REPORT_PERIOD_US) {
            auto send_rate_mbs = last_report_sent_bytes / (double)last_report_period_us;
            last_report_sent_bytes = 0;
            last_report_period_us = 0;

            logger.log(LogLevel::Config, "Send rate: %.3fMB/s", send_rate_mbs);
        }
    }

    for (auto &address : send_addresses) {
        ssize_t bytes_sent = ::sendto(socket, buffer, length, 0,
                                      &address.sa, sizeof(sockaddr));
        if (bytes_sent < 0) {
            logger.log(LogLevel::Debug, "Send error: %s", get_socket_error_string().c_str());
        } else {
            last_sent_bytes = (std::size_t)bytes_sent;
            last_sent_time = clock_type::now();

            if (logger.is_loggable(LogLevel::Debug)) {
                logger.log(LogLevel::Debug, "Sent %zd bytes to %s.", bytes_sent, to_string(address).c_str());
            }
        }
    }
}

UDPReceiver::UDPReceiver(int port, std::string listening_address) :
    logger("transport.receiver"),
    socket(epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
{
    if (socket == INVALID_SOCKET)
    {
        throw std::runtime_error(std::string("Failed to create a socket: ") +
            get_socket_error_string());
    }

    osiSockAddr bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    auto addresses = parse_socket_address_list(listening_address, port);
    if (addresses.size() != 1) {
        throw std::runtime_error(std::string("Invalid bind address: ") +
                listening_address);
    }
    bindAddr = addresses[0];
    logger.log(LogLevel::Debug, "Listening on address: '%s'.", to_string(bindAddr).c_str());

    int status = ::bind(socket, (sockaddr*)&(bindAddr.sa), sizeof(sockaddr));
    if (status)
    {
        epicsSocketDestroy(socket);
        throw std::runtime_error(std::string("Failed to bind socket: ") +
            get_socket_error_string());
    }

    // set timeout
#ifdef _WIN32
    // ms
    DWORD timeout = 250;
#else
    struct timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 250000;
#endif
    status = ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                          (char*)&timeout, sizeof(timeout));
    if (status)
    {
        epicsSocketDestroy(socket);
        throw std::runtime_error(std::string("Error setting SO_RCVTIMEO: ") + 
            get_socket_error_string());
    }
}

UDPReceiver::~UDPReceiver() {
    if (socket != INVALID_SOCKET) {
        epicsSocketDestroy(socket);
    }
}

ssize_t UDPReceiver::receive(const uint8_t* buffer, std::size_t length, osiSockAddr* fromAddress) {
    osiSocklen_t addrStructSize = sizeof(sockaddr);
    
    ssize_t bytes_read = ::recvfrom(socket, (void*)buffer, length, 0,
                                    (sockaddr*)fromAddress, &addrStructSize);
    if (bytes_read > 0) {
        if (logger.is_loggable(LogLevel::Debug)) {
            logger.log(LogLevel::Debug, "Received %zd bytes from %s.", bytes_read, to_string(*fromAddress).c_str());
        }
    }

    return bytes_read;
}


}
