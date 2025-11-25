/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef TEST_SOCKET_UTILS_H
#define TEST_SOCKET_UTILS_H

#include <vector>
#include <cstdint>
#include <string>

#include <epics-diode/transport.h>

namespace test_utils {

class UDPTestSocket {
public:
    UDPTestSocket();
    ~UDPTestSocket();

    // Bind to a specific port (0 = auto-assign)
    bool bind(int port = 0);

    // Get the bound port number
    int get_port() const;

    // Send packet to specific address
    bool send_to(const std::vector<uint8_t>& data, const std::string& host, int port);

    // Receive packet (blocking with timeout)
    ssize_t receive(std::vector<uint8_t>& buffer, int timeout_ms = 1000);

    // Close socket
    void close();

private:
    int socket_fd;
    int bound_port;
    bool is_bound;
};

// Helper to find available port
int find_available_port();

// Helper to create sender/receiver pair
struct SocketPair {
    int sender_port;
    int receiver_port;
    std::string host = "127.0.0.1";
};

SocketPair create_socket_pair();

} // namespace test_utils

#endif // TEST_SOCKET_UTILS_H