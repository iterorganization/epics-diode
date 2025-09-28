/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include "test_sequence_tracker.h"

#include <sstream>
#include <chrono>
#include <condition_variable>
#include <algorithm>
#include <thread>

namespace test_utils {

bool TestResult::sequences_match(const std::vector<uint16_t>& expected) const {
    return received_sequence == expected;
}

std::string TestResult::to_string() const {
    std::ostringstream oss;
    oss << "Sent: [";
    for (size_t i = 0; i < sent_sequence.size(); ++i) {
        if (i > 0) oss << ",";
        oss << sent_sequence[i];
    }
    oss << "] -> Received: [";
    for (size_t i = 0; i < received_sequence.size(); ++i) {
        if (i > 0) oss << ",";
        oss << received_sequence[i];
    }
    oss << "]";

    if (failed) oss << " (FAILED)";
    //if (timeout) oss << " (TIMEOUT)";
    if (!error_message.empty()) oss << " Error: " << error_message;

    return oss.str();
}

PacketSequenceTracker::PacketSequenceTracker() {
}

PacketSequenceTracker::~PacketSequenceTracker() {
}

void PacketSequenceTracker::record_packet(uint16_t seq_no) {
    std::lock_guard<std::mutex> lock(mutex);
    received_sequence.push_back(seq_no);
}

void PacketSequenceTracker::record_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex);
    error_message = error;
    failed = true;
}

std::vector<uint16_t> PacketSequenceTracker::get_sequence() {
    std::lock_guard<std::mutex> lock(mutex);
    return received_sequence;
}

TestResult PacketSequenceTracker::get_result() {
    std::lock_guard<std::mutex> lock(mutex);
    TestResult result;
    result.received_sequence = received_sequence;
    result.failed = failed.load();
    result.timeout = timeout.load();
    result.error_message = error_message;
    return result;
}

void PacketSequenceTracker::reset() {
    std::lock_guard<std::mutex> lock(mutex);
    received_sequence.clear();
    error_message.clear();
    failed = false;
    timeout = false;
}

void PacketSequenceTracker::set_timeout(bool timeout_val) {
    timeout = timeout_val;
}

bool PacketSequenceTracker::has_packets() const {
    std::lock_guard<std::mutex> lock(mutex);
    return !received_sequence.empty();
}

bool PacketSequenceTracker::wait_for_packets(int count, int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout_ms);

    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (received_sequence.size() >= static_cast<size_t>(count)) {
                return true;
            }
        }

        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout_duration) {
            set_timeout(true);
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

CallbackBridge::CallbackBridge(PacketSequenceTracker& tracker, epics_diode::Receiver& receiver)
    : tracker(tracker), receiver(receiver) {
}

void CallbackBridge::operator()(uint32_t channel_id, uint16_t type, uint32_t count, void* value) {
    if (count != (uint32_t)-1) {
        // Record only valid data packets, ignore disconnects
        uint16_t actual_seq_no = receiver.get_current_seq_no();
        tracker.record_packet(actual_seq_no);
    }
}

} // namespace test_utils