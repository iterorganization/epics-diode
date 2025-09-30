/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>
#include <sstream>
#include <map>
#include <set>

#include "testMain.h"
#include "epicsUnitTest.h"
#include "test_socket_utils.h"
#include "test_sequence_tracker.h"

#include <cadef.h>

#include <epics-diode/config.h>
#include <epics-diode/sender.h>
#include <epics-diode/receiver.h>
#include <epics-diode/protocol.h>

namespace edi = epics_diode;

// Helper function to format sequence for display
std::string format_sequence(const std::vector<uint16_t>& seq) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < seq.size(); ++i) {
        if (i > 0) oss << ",";
        oss << seq[i];
    }
    oss << "]";
    return oss.str();
}

// Create a test configuration with single test channel
edi::Config create_test_config() {
    edi::Config config;
    config.heartbeat_period = 1.0;

    edi::ConfigChannel channel;
    channel.channel_name = "TEST:CHANNEL";
    config.channels.push_back(channel);

    return config;
}

// Create a minimal test packet with specific sequence number
std::vector<uint8_t> create_test_packet(uint32_t global_seq_no, uint16_t seq_no) {
    std::vector<uint8_t> packet;

    // Header - use constructor to set magic, startup_time, config_hash, and global sequence properly
    edi::Header header(1000, 0, global_seq_no);

    // Submessage header
    edi::SubmessageHeader sub_header;
    sub_header.id = edi::SubmessageType::CA_DATA_MESSAGE;
    sub_header.flags = edi::SubmessageFlag::LittleEndian;
    sub_header.bytes_to_next_header = 0;

    // CA Data message
    edi::CADataMessage data_msg;
    data_msg.seq_no = seq_no;
    data_msg.channel_count = 1;

    // Channel data
    edi::CAChannelData channel_data;
    channel_data.id = 0;
    channel_data.type = DBR_STRING;
    channel_data.count = 1;

    // Build packet
    packet.resize(sizeof(edi::Header) + sizeof(edi::SubmessageHeader) +
                  sizeof(edi::CADataMessage) + sizeof(edi::CAChannelData) + 40);

    size_t offset = 0;
    std::memcpy(packet.data() + offset, &header, sizeof(header));
    offset += sizeof(header);
    std::memcpy(packet.data() + offset, &sub_header, sizeof(sub_header));
    offset += sizeof(sub_header);
    std::memcpy(packet.data() + offset, &data_msg, sizeof(data_msg));
    offset += sizeof(data_msg);
    std::memcpy(packet.data() + offset, &channel_data, sizeof(channel_data));
    offset += sizeof(channel_data);

    // Add dummy string data
    const char* test_data = "test";
    std::memcpy(packet.data() + offset, test_data, strlen(test_data) + 1);

    return packet;
}

// Create a fragmented test packet
std::vector<uint8_t> create_frag_test_packet(uint32_t global_seq_no, uint16_t msg_seq_no, uint16_t fragment_seq_no) {
    std::vector<uint8_t> packet;

    // Header - use global sequence number for packet ordering
    edi::Header header(1000, 0, global_seq_no);

    // Submessage header
    edi::SubmessageHeader sub_header;
    sub_header.id = edi::SubmessageType::CA_FRAG_DATA_MESSAGE;
    sub_header.flags = edi::SubmessageFlag::LittleEndian;
    sub_header.bytes_to_next_header = 0;

    // CA Fragment Data message - all fragments of same message use same seq_no
    edi::CAFragDataMessage frag_msg;
    frag_msg.seq_no = msg_seq_no;  // Same for all fragments of this message
    frag_msg.fragment_seq_no = fragment_seq_no;
    frag_msg.channel_id = 0;
    frag_msg.type = DBR_STRING;
    frag_msg.count = 1; // Total count for complete assembled string

    // Fragment data - create a 40-byte message split into 3 fragments for DBR_STRING
    // DBR_STRING with count=1 expects 40 bytes total (MAX_STRING_SIZE)
    const char* full_message = "TestFragmentedMessageData0123456789ABCDEF"; // 39 chars + null = 40 bytes

    // Split into 3 fragments: 13, 13, 14 bytes (including null terminator for last fragment)
    size_t frag_start[] = {0, 13, 26};
    size_t frag_sizes[] = {13, 13, 14}; // Last fragment includes null terminator

    frag_msg.fragment_size = frag_sizes[fragment_seq_no];
    const char* frag_data = full_message + frag_start[fragment_seq_no];

    // Build packet
    packet.resize(sizeof(edi::Header) + sizeof(edi::SubmessageHeader) +
                  sizeof(edi::CAFragDataMessage) + frag_msg.fragment_size);

    size_t offset = 0;
    std::memcpy(packet.data() + offset, &header, sizeof(header));
    offset += sizeof(header);
    std::memcpy(packet.data() + offset, &sub_header, sizeof(sub_header));
    offset += sizeof(sub_header);
    std::memcpy(packet.data() + offset, &frag_msg, sizeof(frag_msg));
    offset += sizeof(frag_msg);

    // Add fragment data
    std::memcpy(packet.data() + offset, frag_data, frag_msg.fragment_size);

    return packet;
}

// Test harness for sender/receiver testing
class SenderReceiverTestHarness {
private:
    std::unique_ptr<edi::Receiver> receiver;
    std::unique_ptr<std::thread> receiver_thread;
    test_utils::UDPTestSocket sender_socket;
    test_utils::PacketSequenceTracker tracker;
    std::atomic<bool> stop_flag{false};
    int receiver_port;

public:
    SenderReceiverTestHarness() : receiver_port(0) {}

    ~SenderReceiverTestHarness() {
        cleanup();
    }

    bool setup() {
        // Find available port for receiver
        receiver_port = test_utils::find_available_port();
        if (receiver_port < 0) {
            return false;
        }

        // Create receiver with test config
        auto config = create_test_config();
        try {
            receiver.reset(new edi::Receiver(config, receiver_port, "127.0.0.1"));
        } catch (const std::exception& e) {
            testDiag("Failed to create receiver: %s", e.what());
            return false;
        }

        // Setup sender socket
        if (!sender_socket.bind(0)) {
            return false;
        }

        return true;
    }

    void cleanup() {
        stop_flag = true;
        if (receiver_thread && receiver_thread->joinable()) {
            receiver_thread->join();
        }
        receiver.reset();
        sender_socket.close();
    }

    test_utils::TestResult run_fragment_test(const std::vector<uint16_t>& sequence,
                                              const std::vector<bool>& is_fragment,
                                              const std::vector<uint16_t>& frag_seq_nos = {},
                                              int packet_delay_ms = 50,
                                              int test_timeout_ms = 1000) {
        tracker.reset();
        stop_flag = false;

        // Start receiver in background thread
        receiver_thread.reset(new std::thread([this]() {
            try {
                test_utils::CallbackBridge bridge(tracker, *receiver);
                auto callback = std::ref(bridge);

                // Run receiver for test duration
                receiver->run(1.0, callback); // 1 second runtime
            } catch (const std::exception& e) {
                tracker.record_error(std::string("Receiver exception: ") + e.what());
            }
        }));

        // Give receiver time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Send test sequence with mixed regular and fragmented packets
        test_utils::TestResult result;
        result.sent_sequence = sequence;

        uint32_t global_seq_no = 1;
        for (size_t i = 0; i < sequence.size(); ++i) {
            uint16_t seq_no = sequence[i];
            std::vector<uint8_t> packet;

            if (i < is_fragment.size() && is_fragment[i]) {
                // Create fragmented packet
                uint16_t frag_seq = (i < frag_seq_nos.size()) ? frag_seq_nos[i] : 0;
                packet = create_frag_test_packet(global_seq_no++, seq_no, frag_seq);
                testDiag("Sent fragment packet %u (frag_seq=%u)", seq_no, frag_seq);
            } else {
                // Create regular packet
                packet = create_test_packet(global_seq_no++, seq_no);
                testDiag("Sent packet %u", seq_no);
            }

            if (!sender_socket.send_to(packet, "127.0.0.1", receiver_port)) {
                result.error_message = "Failed to send packet " + std::to_string(seq_no);
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(packet_delay_ms));
        }

        // Wait for processing or timeout
        // Fragment tests may have fewer received packets than sent
        size_t expected_count = 0;

        // Count regular (non-fragment) packets
        for (size_t i = 0; i < sequence.size(); ++i) {
            if (i >= is_fragment.size() || !is_fragment[i]) {
                expected_count++; // Regular packets
            }
        }

        // Check if we have complete fragment sequences (0, 1, 2 for each sequence number)
        if (!is_fragment.empty()) {
            // Find fragment sequence numbers and check if we have complete sets
            std::map<uint16_t, std::set<uint16_t>> fragment_map;
            for (size_t i = 0; i < sequence.size(); ++i) {
                if (i < is_fragment.size() && is_fragment[i]) {
                    uint16_t seq_no = sequence[i];
                    uint16_t frag_seq = (i < frag_seq_nos.size()) ? frag_seq_nos[i] : 0;
                    fragment_map[seq_no].insert(frag_seq);
                }
            }

            // Check each fragment sequence for completeness (needs 0, 1, 2)
            for (const auto& pair : fragment_map) {
                const std::set<uint16_t>& frags = pair.second;
                if (frags.size() == 3 && frags.count(0) && frags.count(1) && frags.count(2)) {
                    expected_count++; // Complete fragment sequence produces one callback
                }
            }
        }

        testDiag("Expecting %zu packets (regular + complete fragments)", expected_count);

         // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(test_timeout_ms));

        // Stop receiver
        stop_flag = true;

        // Get results
        result = tracker.get_result();
        result.sent_sequence = sequence;

        // Cleanup for next test
        if (receiver_thread && receiver_thread->joinable()) {
            receiver_thread->join();
        }
        receiver_thread.reset();

        return result;
    }


    test_utils::TestResult run_sequence_test(const std::vector<uint16_t>& sequence,
                                            int packet_delay_ms = 50,
                                            int test_timeout_ms = 1000) {
        tracker.reset();
        stop_flag = false;

        // Start receiver in background thread
        receiver_thread.reset(new std::thread([this]() {
            try {
                test_utils::CallbackBridge bridge(tracker, *receiver);
                auto callback = std::ref(bridge);

                // Run receiver for test duration
                receiver->run(1.0, callback); // 1 second runtime
            } catch (const std::exception& e) {
                tracker.record_error(std::string("Receiver exception: ") + e.what());
            }
        }));

        // Give receiver time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Send test sequence
        test_utils::TestResult result;
        result.sent_sequence = sequence;

        for (uint16_t seq_no : sequence) {
            auto packet = create_test_packet(seq_no, seq_no);

            if (!sender_socket.send_to(packet, "127.0.0.1", receiver_port)) {
                result.error_message = "Failed to send packet " + std::to_string(seq_no);
                break;
            }

            testDiag("Sent packet %u", seq_no);
            std::this_thread::sleep_for(std::chrono::milliseconds(packet_delay_ms));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(test_timeout_ms));

        // Stop receiver
        stop_flag = true;

        // Get results
        result = tracker.get_result();
        result.sent_sequence = sequence;

        // Cleanup for next test
        if (receiver_thread && receiver_thread->joinable()) {
            receiver_thread->join();
        }
        receiver_thread.reset();

        return result;
    }

    test_utils::TestResult run_custom_global_seq_test(const std::vector<uint32_t>& global_seq_nos,
                                                       const std::vector<uint16_t>& sequence,
                                                       int packet_delay_ms = 50,
                                                       int test_timeout_ms = 1000) {
        tracker.reset();
        stop_flag = false;

        // Start receiver in background thread
        receiver_thread.reset(new std::thread([this]() {
            try {
                test_utils::CallbackBridge bridge(tracker, *receiver);
                auto callback = std::ref(bridge);
                receiver->run(1.0, callback);
            } catch (const std::exception& e) {
                tracker.record_error(std::string("Receiver exception: ") + e.what());
            }
        }));

        // Give receiver time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Send test sequence with custom global sequence numbers
        test_utils::TestResult result;
        result.sent_sequence = sequence;

        for (size_t i = 0; i < sequence.size(); ++i) {
            auto packet = create_test_packet(global_seq_nos[i], sequence[i]);

            if (!sender_socket.send_to(packet, "127.0.0.1", receiver_port)) {
                result.error_message = "Failed to send packet " + std::to_string(i);
                break;
            }

            testDiag("Sent packet with global_seq_no=0x%08X, msg_seq=%u", global_seq_nos[i], sequence[i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(packet_delay_ms));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(test_timeout_ms));

        // Stop receiver
        stop_flag = true;

        // Get results
        result = tracker.get_result();
        result.sent_sequence = sequence;

        // Cleanup for next test
        if (receiver_thread && receiver_thread->joinable()) {
            receiver_thread->join();
        }
        receiver_thread.reset();

        return result;
    }
};

// Test normal sequence processing
static void test_normal_sequence() {
    testDiag("Testing normal sequence 1,2,3,4...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness");
        return;
    }

    std::vector<uint16_t> sequence = {1, 2, 3, 4};
    auto result = harness.run_sequence_test(sequence);

    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Normal sequence test failed: %s", result.error_message.c_str());
    } else if (result.received_sequence.size() == sequence.size()) {
        testPass("Normal sequence processed correctly");
    } else {
        testFail("Normal sequence: expected %zu packets, got %zu",
                sequence.size(), result.received_sequence.size());
    }
}

// Test simple adjacent reordering - should trigger the Serializer bug
static void test_simple_reorder() {
    testDiag("Testing simple reorder 1,2,4,3...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for reorder test");
        return;
    }

    std::vector<uint16_t> sequence = {1, 2, 4, 3};
    std::vector<uint16_t> expected = {1, 2, 3, 4}; // Expected after reordering

    auto result = harness.run_sequence_test(sequence);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Simple reorder test failed: %s",
                result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Simple reorder worked correctly");
    } else {
        testFail("Simple reorder: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test sequence break while holding - should trigger the Serializer bug
static void test_hold_and_break() {
    testDiag("Testing hold and break 1,2,4,5...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for hold/break test");
        return;
    }

    std::vector<uint16_t> sequence = {1, 2, 4, 5};
    std::vector<uint16_t> expected = {1, 2, 4, 5}; // Should maintain order

    auto result = harness.run_sequence_test(sequence);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Hold and break test failed: %s",
                result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Hold and break worked correctly");
    } else {
        testFail("Hold and break: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test gap with reorder - should process only first few
static void test_gap_with_reorder() {
    testDiag("Testing gap with reorder 1,2,5,3,4...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for gap test");
        return;
    }

    std::vector<uint16_t> sequence = {1, 2, 5, 3, 4};
    std::vector<uint16_t> expected = {1, 2, 5}; // Only 1,2,5 processed (3,4 dropped as "old")

    auto result = harness.run_sequence_test(sequence);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Gap with reorder test failed: %s", result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Gap with reorder worked correctly");
    } else {
        testFail("Gap with reorder: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test larger gap sequence
static void test_larger_gap() {
    testDiag("Testing larger gap 1,2,5,6...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for larger gap test");
        return;
    }

    std::vector<uint16_t> sequence = {1, 2, 5, 6};
    std::vector<uint16_t> expected = {1, 2, 5, 6}; // Should maintain order

    auto result = harness.run_sequence_test(sequence);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Larger gap test failed: %s", result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Larger gap worked correctly");
    } else {
        testFail("Larger gap: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test massive reorder
static void test_massive_reorder() {
    testDiag("Testing massive reorder 1,5,2,3,4...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for massive reorder test");
        return;
    }

    std::vector<uint16_t> sequence = {1, 5, 2, 3, 4};
    std::vector<uint16_t> expected = {1, 5}; // Only 1,5 processed (2,3,4 would be "old")

    auto result = harness.run_sequence_test(sequence);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Massive reorder test failed: %s", result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Massive reorder worked correctly");
    } else {
        testFail("Massive reorder: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test complete fragment sequence: 1,2,3(frag1st),3(frag2nd),3(frag3rd)
static void test_frag_complete() {
    testDiag("Testing complete fragments 1,2,3(frag1st),3(frag2nd),3(frag3rd)...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for complete fragment test");
        return;
    }

    std::vector<uint16_t> sequence = {1, 2, 3, 3, 3}; // All three fragments of message 3
    std::vector<bool> is_fragment = {false, false, true, true, true};
    std::vector<uint16_t> frag_seq_nos = {0, 0, 0, 1, 2}; // Fragment sequence 0,1,2

    std::vector<uint16_t> expected = {1, 2, 3}; // Complete fragments should assemble message 3

    auto result = harness.run_fragment_test(sequence, is_fragment, frag_seq_nos);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Complete fragment test failed: %s", result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Complete fragment sequence worked correctly");
    } else {
        testFail("Complete fragment: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test missing middle fragment: 1,2,3(frag1st),3(frag3rd) - should drop message 3
static void test_frag_missing_middle() {
    testDiag("Testing missing middle fragment 1,2,3(frag1st),3(frag3rd)...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for missing middle fragment test");
        return;
    }

    std::vector<uint16_t> sequence = {1, 2, 3, 3}; // Missing fragment 1 (middle)
    std::vector<bool> is_fragment = {false, false, true, true};
    std::vector<uint16_t> frag_seq_nos = {0, 0, 0, 2}; // Fragment sequence 0,2 (missing 1)

    std::vector<uint16_t> expected = {1, 2}; // Message 3 should be dropped

    auto result = harness.run_fragment_test(sequence, is_fragment, frag_seq_nos);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Missing middle fragment test failed: %s", result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Missing middle fragment correctly dropped message");
    } else {
        testFail("Missing middle fragment: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test missing last fragment: 1,2,3(frag1st),3(frag2nd),4 - should drop message 3
static void test_frag_missing_last() {
    testDiag("Testing missing last fragment 1,2,3(frag1st),3(frag2nd),4...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for missing last fragment test");
        return;
    }

    std::vector<uint16_t> sequence = {1, 2, 3, 3, 4}; // Missing fragment 2 (last), then packet 4
    std::vector<bool> is_fragment = {false, false, true, true, false};
    std::vector<uint16_t> frag_seq_nos = {0, 0, 0, 1, 0}; // Fragment sequence 0,1 (missing 2)

    std::vector<uint16_t> expected = {1, 2, 4}; // Message 3 dropped, packet 4 processed

    auto result = harness.run_fragment_test(sequence, is_fragment, frag_seq_nos);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Missing last fragment test failed: %s", result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Missing last fragment correctly dropped message");
    } else {
        testFail("Missing last fragment: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test missing first fragment: 1,2,3(frag2nd),3(frag3rd),4 - should drop message 3
static void test_frag_missing_first() {
    testDiag("Testing missing first fragment 1,2,3(frag2nd),3(frag3rd),4...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for missing first fragment test");
        return;
    }

    std::vector<uint16_t> sequence = {1, 2, 3, 3, 4}; // Missing fragment 0 (first), then packet 4
    std::vector<bool> is_fragment = {false, false, true, true, false};
    std::vector<uint16_t> frag_seq_nos = {0, 0, 1, 2, 0}; // Fragment sequence 1,2 (missing 0)

    std::vector<uint16_t> expected = {1, 2, 4}; // Message 3 dropped, packet 4 processed

    auto result = harness.run_fragment_test(sequence, is_fragment, frag_seq_nos);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Missing first fragment test failed: %s", result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Missing first fragment correctly dropped message");
    } else {
        testFail("Missing first fragment: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test global sequence number wrap-around at 2^32
static void test_global_seq_wraparound() {
    testDiag("Testing global sequence wrap-around: 0xFFFFFFFE, 0xFFFFFFFF, 1, 0, 2...");

    SenderReceiverTestHarness harness;
    if (!harness.setup()) {
        testFail("Failed to setup test harness for wrap-around test");
        return;
    }

    // Test that wrap-around comparison works correctly:
    // Sequence: 0xFFFFFFFD -> 0xFFFFFFFE -> 0xFFFFFFFF -> 0 -> 1
    // All in order, crossing the wrap point at 2^32
    // The fix ensures (int32_t)(0 - 0xFFFFFFFF) = 1 > 0, so packet 0 is newer
    std::vector<uint32_t> global_seq_nos = {0xFFFFFFFD, 0xFFFFFFFE, 0xFFFFFFFF, 0, 1};
    std::vector<uint16_t> sequence = {1, 2, 3, 4, 5}; // Message sequence numbers
    std::vector<uint16_t> expected = {1, 2, 3, 4, 5}; // All should be processed

    auto result = harness.run_custom_global_seq_test(global_seq_nos, sequence);

    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Wrap-around test failed: %s", result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Global sequence wrap-around handled correctly");
    } else {
        testFail("Wrap-around: expected %s but got %s",
                format_sequence(expected).c_str(), format_sequence(result.received_sequence).c_str());
    }
}

// Test packet creation and basic structures
static void test_packet_creation() {
    testDiag("Testing packet creation...");

    auto packet = create_test_packet(42, 1);

    if (packet.size() > 0) {
        testPass("Packet creation successful");
    } else {
        testFail("Packet creation failed");
    }

    // Verify packet structure
    if (packet.size() >= sizeof(edi::Header)) {
        edi::Header* header = reinterpret_cast<edi::Header*>(packet.data());
        // Since we moved to global sequence numbers, just check that header was created properly
        if (header->global_seq_no == 42) {
            testPass("Header global sequence correct");
        } else {
            testFail("Header global sequence incorrect: expected 42, got %u", header->global_seq_no);
        }
    } else {
        testFail("Packet too small for header");
    }
}

MAIN(test_sender_receiver) {
    testPlan(13);

    testDiag("=== Sender/Receiver Unit Tests ===");
    testDiag("Testing packet reordering and sequence validation");

    try {
        // Test 1: Basic packet creation
        test_packet_creation();

        // Test 2: Normal sequence
        test_normal_sequence();

        // Test 3: Simple reorder
        test_simple_reorder();

        // Test 4: Hold and break
        test_hold_and_break();

        // Test 5: Gap with reorder
        test_gap_with_reorder();

        // Test 6: Larger gap
        test_larger_gap();

        // Test 7: Massive reorder
        test_massive_reorder();

        // Test 8: Complete fragment sequence
        test_frag_complete();

        // Test 9: Missing middle fragment
        test_frag_missing_middle();

        // Test 10: Missing last fragment
        test_frag_missing_last();

        // Test 11: Missing first fragment
        test_frag_missing_first();

        // Test 12: Global sequence wrap-around
        test_global_seq_wraparound();

    } catch (std::exception& e) {
        testFail("Exception: %s", e.what());
    }

    return testDone();
}