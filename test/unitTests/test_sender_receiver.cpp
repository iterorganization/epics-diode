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
std::vector<uint8_t> create_test_packet(uint16_t seq_no) {
    std::vector<uint8_t> packet;

    // Header - use constructor to set magic and version properly
    edi::Header header(1000, 0);

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

    test_utils::TestResult run_sequence_test(const std::vector<uint16_t>& sequence,
                                            int packet_delay_ms = 50,
                                            int test_timeout_ms = 1500) {
        tracker.reset();
        stop_flag = false;

        // Start receiver in background thread
        receiver_thread.reset(new std::thread([this]() {
            try {
                test_utils::CallbackBridge bridge(tracker);
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
            auto packet = create_test_packet(seq_no);

            if (!sender_socket.send_to(packet, "127.0.0.1", receiver_port)) {
                result.error_message = "Failed to send packet " + std::to_string(seq_no);
                break;
            }

            testDiag("Sent packet %u", seq_no);
            std::this_thread::sleep_for(std::chrono::milliseconds(packet_delay_ms));
        }

        // Wait for processing or timeout
        if (!tracker.wait_for_packets(sequence.size(), test_timeout_ms)) {
            testDiag("Test timed out waiting for packets");
        }

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
    } else if (result.timeout) {
        testFail("Normal sequence test timed out");
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

    testTodoBegin("Simple reordering test");

    auto result = harness.run_sequence_test(sequence);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Simple reorder test failed: %s",
                result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Simple reorder worked correctly");
    } else {
        testFail("Simple reorder: unexpected sequence received");
    }

    testTodoEnd();
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

    testTodoBegin("Hold and break test");

    auto result = harness.run_sequence_test(sequence);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Hold and break test failed: %s",
                result.error_message.c_str());
    } else if (result.sequences_match(expected)) {
        testPass("Hold and break worked correctly");
    } else {
        testFail("Hold and break: unexpected sequence received");
    }

    testTodoEnd();
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
    // Expected: only 1,2,5 should be processed (3,4 dropped as "old")

    auto result = harness.run_sequence_test(sequence);
    testDiag("Result: %s", result.to_string().c_str());

    if (result.failed) {
        testFail("Gap with reorder test failed: %s", result.error_message.c_str());
    } else if (result.received_sequence.size() >= 3) {
        // We expect at least the first 3 packets (1,2,5) to be processed
        testPass("Gap with reorder test completed");
    } else {
        testFail("Gap with reorder: insufficient packets received");
    }
}

// Test packet creation and basic structures
static void test_packet_creation() {
    testDiag("Testing packet creation...");

    auto packet = create_test_packet(42);

    if (packet.size() > 0) {
        testPass("Packet creation successful");
    } else {
        testFail("Packet creation failed");
    }

    // Verify packet structure
    if (packet.size() >= sizeof(edi::Header)) {
        edi::Header* header = reinterpret_cast<edi::Header*>(packet.data());
        if (header->version == edi::Header::VERSION) {
            testPass("Header version correct");
        } else {
            testFail("Header version incorrect");
        }
    } else {
        testFail("Packet too small for header");
    }
}

MAIN(test_sender_receiver) {
    testPlan(5);

    testDiag("=== Sender/Receiver Unit Tests ===");
    testDiag("Testing packet reordering and sequence validation");

    try {
        // Test 1: Basic packet creation
        test_packet_creation();

        // Test 2: Normal sequence (should eventually pass)
        test_normal_sequence();

        // Test 3: Simple reorder (expected to fail due to bug)
        test_simple_reorder();

        // Test 4: Hold and break (expected to fail due to bug)
        test_hold_and_break();

        // Test 5: Gap with reorder
        test_gap_with_reorder();

    } catch (std::exception& e) {
        testFail("Exception: %s", e.what());
    }

    return testDone();
}