/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include "test_socket_utils.h"

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

namespace test_utils {

UDPTestSocket::UDPTestSocket()
    : socket_fd(-1), bound_port(0), is_bound(false) {
}

UDPTestSocket::~UDPTestSocket() {
    close();
}

bool UDPTestSocket::bind(int port) {
    // Create socket
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        return false;
    }

    // Set socket options
    int reuse = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Bind to address
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    if (::bind(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(socket_fd);
        socket_fd = -1;
        return false;
    }

    // Get actual bound port
    socklen_t addr_len = sizeof(addr);
    if (getsockname(socket_fd, (sockaddr*)&addr, &addr_len) == 0) {
        bound_port = ntohs(addr.sin_port);
        is_bound = true;
        return true;
    }

    ::close(socket_fd);
    socket_fd = -1;
    return false;
}

int UDPTestSocket::get_port() const {
    return bound_port;
}

bool UDPTestSocket::send_to(const std::vector<uint8_t>& data, const std::string& host, int port) {
    if (socket_fd < 0) {
        return false;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    addr.sin_port = htons(port);

    ssize_t sent = sendto(socket_fd, data.data(), data.size(), 0,
                         (sockaddr*)&addr, sizeof(addr));
    return sent == (ssize_t)data.size();
}

ssize_t UDPTestSocket::receive(std::vector<uint8_t>& buffer, int timeout_ms) {
    if (socket_fd < 0) {
        return -1;
    }

    // Set timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(socket_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (result <= 0) {
        return result; // timeout or error
    }

    // Receive data
    sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    buffer.resize(65536); // Max UDP packet size
    ssize_t received = recvfrom(socket_fd, buffer.data(), buffer.size(), 0,
                               (sockaddr*)&from_addr, &from_len);

    if (received > 0) {
        buffer.resize(received);
    }

    return received;
}

void UDPTestSocket::close() {
    if (socket_fd >= 0) {
        ::close(socket_fd);
        socket_fd = -1;
        is_bound = false;
        bound_port = 0;
    }
}

int find_available_port() {
    UDPTestSocket socket;
    if (socket.bind(0)) { // Auto-assign port
        int port = socket.get_port();
        socket.close();
        return port;
    }
    return -1;
}

SocketPair create_socket_pair() {
    SocketPair pair;
    pair.sender_port = find_available_port();
    pair.receiver_port = find_available_port();
    return pair;
}

} // namespace test_utils