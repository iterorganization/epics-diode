/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef TEST_SEQUENCE_TRACKER_H
#define TEST_SEQUENCE_TRACKER_H

#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <string>

#include <epics-diode/receiver.h>

namespace test_utils {

struct TestResult {
    std::vector<uint16_t> sent_sequence;
    std::vector<uint16_t> received_sequence;
    bool failed = false;
    bool timeout = false;
    std::string error_message;

    bool sequences_match(const std::vector<uint16_t>& expected) const;
    std::string to_string() const;
};

class PacketSequenceTracker {
public:
    PacketSequenceTracker();
    ~PacketSequenceTracker();

    // Record a received packet sequence number
    void record_packet(uint16_t seq_no);

    // Record an error or crash
    void record_error(const std::string& error);

    // Get the recorded sequence (thread-safe)
    std::vector<uint16_t> get_sequence();

    // Get full test result
    TestResult get_result();

    // Reset for new test
    void reset();

    // Set timeout flag
    void set_timeout(bool timeout = true);

    // Check if any packets have been received
    bool has_packets() const;

    // Wait for at least N packets or timeout
    bool wait_for_packets(int count, int timeout_ms);

private:
    mutable std::mutex mutex;
    std::vector<uint16_t> received_sequence;
    std::string error_message;
    std::atomic<bool> failed{false};
    std::atomic<bool> timeout{false};
};

// Callback function for receiver to track packets
// This is the bridge between the receiver callback and our tracker
class CallbackBridge {
public:
    explicit CallbackBridge(PacketSequenceTracker& tracker, epics_diode::Receiver& receiver);

    // The actual callback function
    void operator()(uint32_t channel_id, uint16_t type, uint32_t count, void* value);

private:
    PacketSequenceTracker& tracker;
    epics_diode::Receiver& receiver;  // Reference to get actual sequence numbers
};

} // namespace test_utils

#endif // TEST_SEQUENCE_TRACKER_H