/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <cadef.h>

#include <epics-diode/config.h>
#include <epics-diode/logger.h>
#include <epics-diode/protocol.h>
#include <epics-diode/receiver.h>
#include <epics-diode/transport.h>
#include <epics-diode/version.h>

namespace epics_diode {

struct Receiver::Impl {
    Impl(const epics_diode::Config& config, int port, std::string listening_address);
    void run(double runtime, Callback callback);

    // For testing: get sequence number of packet currently being processed
    uint16_t get_current_seq_no() const { return current_processing_seq_no; }

private:
    Logger logger;

    using clock_type = std::chrono::steady_clock;
    std::chrono::time_point<clock_type> current_update_time{};
    std::chrono::time_point<clock_type> last_heartbeat_time;

    struct Channel {

        Channel(std::uint32_t id, const std::string& name) :
            id(id), name(name) {
        }

        std::uint32_t id;
        std::string name;
        bool disconnected = false;  // we want disconnected event to be sent after start (if no updated within heartbeat period)
        std::chrono::time_point<clock_type> last_update_time{};
    };

    static constexpr std::size_t MAX_CA_DATA_SIZE = 16 * 1024 * 1024;   

    UDPReceiver initialize_receiver(int port, std::string listening_address, const Config& config);
    std::vector<Channel> create_channels(const Config& config);

    bool validate_fragment_sequence(uint16_t seq_no, uint16_t fragment_seq_no);
    bool validate_sender(uint64_t startup_time);
    ssize_t receive_updates(const Callback& callback);
    ssize_t process_packet_data(const uint8_t* packet_data, ssize_t bytes_received, const Callback& callback, const osiSockAddr& fromAddress);
    void check_no_updates(Callback callback);

    std::size_t config_hash;
    double heartbeat_period;
    std::vector<Serializer::value_type> receive_buffer;
    std::vector<Serializer::value_type> fragment_buffer;
    Serializer fragment_serializer;
    
    // Buffer swapping for out-of-order packet handling
    std::vector<Serializer::value_type> held_packet;
    ssize_t held_bytes = 0;
    uint32_t held_seq_no = 0;

    UDPReceiver receiver;

    uint16_t active_fragment_seq_no = (uint16_t)-1;
    uint16_t last_fragment_seq_no = (uint16_t)-1;
    uint64_t last_startup_time = 0;
    uint32_t last_global_seq_no = (uint32_t)-1;

    // For testing: tracks sequence number of packet currently being processed
    uint16_t current_processing_seq_no = 0;

    std::vector<Channel> channels;
};

Receiver::Impl::Impl(const Config& config, int port, std::string listening_address) :
    logger("receiver"),
    last_heartbeat_time(clock_type::now()),
    config_hash(config.hash),
    heartbeat_period(config.heartbeat_period),
    receive_buffer(MAX_MESSAGE_SIZE),
    fragment_buffer(MAX_CA_DATA_SIZE),
    fragment_serializer(fragment_buffer.data(), 0),
    held_packet(MAX_MESSAGE_SIZE),
    receiver(initialize_receiver(port, listening_address, config)),
    channels(create_channels(config))
{
}

void Receiver::Impl::check_no_updates(Callback callback) {
    using secs = std::chrono::seconds;

    auto diff_hb_seconds = std::chrono::duration_cast<secs>(current_update_time - last_heartbeat_time).count();
    if (diff_hb_seconds >= heartbeat_period) {
        double invalidate_period = 2 * heartbeat_period;
        for (auto &channel : channels) {
            if (!channel.disconnected &&
                std::chrono::duration_cast<secs>(current_update_time - channel.last_update_time).count() >= invalidate_period) {

                // mark as disconnected and call callback
                channel.disconnected = true;

                // guarded callback call
                try {
                    callback(channel.id, 0, (uint32_t)-1, 0);
                } catch (std::exception& ex) {
                    logger.log(LogLevel::Error, "Exception escaped out of callback: %s", ex.what());
                }
            }
        }
        last_heartbeat_time = current_update_time;
    }
}

void Receiver::Impl::run(double runtime, Callback callback) {
    using secs = std::chrono::seconds;

    auto start = clock_type::now();
    while (1) {

        // process packets
        int max_packets_at_once = 100;
        while (receive_updates(callback) > 0 && --max_packets_at_once);

        current_update_time = clock_type::now();
        
        check_no_updates(callback);

        if (runtime > 0) {
            if (std::chrono::duration_cast<secs>(current_update_time - start).count() >= runtime) {
                break;
            }
        }
    }
}

UDPReceiver Receiver::Impl::initialize_receiver(int port, std::string listening_address, const Config& config)
{
    logger.log(LogLevel::Info, "Initializing transport, listening at '%s:%d'.", listening_address.c_str(), port);

    assert(receive_buffer.size() % SubmessageHeader::alignment == 0);

    return UDPReceiver(port, listening_address);
}

std::vector<Receiver::Impl::Channel> Receiver::Impl::create_channels(const Config& config)
{
    logger.log(LogLevel::Info, "Creating %u channels.", config.total_channel_count());

    assert(config.total_channel_count() <= std::numeric_limits<uint32_t>::max());

    std::vector<Channel> channels;
    channels.reserve(config.total_channel_count()); 
    uint32_t current_channel_num = 0;

    for (auto &config_channel : config.channels) {
        // Create channel for default field.
        channels.emplace_back(Channel{
                current_channel_num++,
                config_channel.channel_name
        });

        for (auto &field_name : config_channel.extra_fields) {
            channels.emplace_back(Channel{
                    current_channel_num++,
                    config_channel.channel_name + "." + field_name
            });
        }
        for (auto &field_name : config_channel.polled_fields) {
            channels.emplace_back(Channel{
                    current_channel_num++,
                    config_channel.channel_name + "." + field_name
            });
        }
    }

    return channels;
}



bool Receiver::Impl::validate_fragment_sequence(uint16_t seq_no, uint16_t fragment_seq_no) {
    // first fragment
    if (fragment_seq_no == 0) {
        // Start new fragment sequence
        active_fragment_seq_no = seq_no;
        last_fragment_seq_no = 0;
        return true;
    } else {
        // check if the same as currently active fragment
        if (active_fragment_seq_no != seq_no) {
            active_fragment_seq_no = (uint16_t)-1;
            return false;
        }

        // next fragment received check
        if (++last_fragment_seq_no == fragment_seq_no) {
            return true;
        } else {
            active_fragment_seq_no = (uint16_t)-1;
            return false;
        }
    }
}

bool Receiver::Impl::validate_sender(uint64_t startup_time) {
    
    if (startup_time == last_startup_time) {
        return true;
    } else if (startup_time > last_startup_time) {
        last_startup_time = startup_time;
        // reset global seq_no
        last_global_seq_no = (uint32_t)-1;
        return true;
    } else {
        // reject older senders
        return false;
    }
}

ssize_t Receiver::Impl::receive_updates(const Callback& callback) {
    osiSockAddr fromAddress;

    // Always receive a new packet from socket first
    ssize_t bytes_received = receiver.receive(receive_buffer.data(), receive_buffer.size(), &fromAddress);
    if (bytes_received <= 0) {
        return bytes_received;
    }

    Serializer s(receive_buffer.data(), (std::size_t)bytes_received);
    Header header;
    s >> header;

    if (!header.validate()) {
        logger.log(LogLevel::Warning, "Invalid header received from '%s'.",
                   to_string(fromAddress).c_str());
        return bytes_received;
    }

    if (header.config_hash != config_hash) {
        logger.log(LogLevel::Warning, "Configuration mismatch to sender at '%s'.",
                   to_string(fromAddress).c_str());
        return bytes_received;
    }

    if (!validate_sender(header.startup_time)) {
        logger.log(LogLevel::Warning, "Multiple senders detected, rejecting older sender at '%s'.",
                   to_string(fromAddress).c_str());
        return bytes_received;
    }

    uint32_t global_seq_no = header.global_seq_no;

    // First packet - initialize and process
    if (last_global_seq_no == (uint32_t)-1) {
        last_global_seq_no = global_seq_no;
        return process_packet_data(receive_buffer.data(), bytes_received, callback, fromAddress);
    }

    uint32_t expected = last_global_seq_no + 1;

    // Use signed arithmetic to handle wrap-around at 2^32
    // When global_seq_no wraps from 0xFFFFFFFF to 0, simple comparison fails:
    // (0 <= 0xFFFFFFFF) would incorrectly treat 0 as old
    // Signed difference handles this: (int32_t)(0 - 0xFFFFFFFF) = 1 (positive, newer)
    if ((int32_t)(global_seq_no - last_global_seq_no) <= 0) {
        // Packet is old/duplicate - drop it
        logger.log(LogLevel::Debug, "Dropped old/duplicate packet: seq %u (expected > %u)",
                  global_seq_no, last_global_seq_no);
        return bytes_received;
    }

    // In-order packet arrival
    if (global_seq_no == expected) {
        process_packet_data(receive_buffer.data(), bytes_received, callback, fromAddress);
        if (held_bytes > 0) {
            // Process the held packet after the expected one
            process_packet_data(held_packet.data(), held_bytes, callback, fromAddress);
            last_global_seq_no = held_seq_no;
            held_bytes = 0;
        } else {
            last_global_seq_no = global_seq_no;
        }
        return bytes_received;
    }

    // Out-of-order packet: exactly one ahead and not holding - hold it
    if (global_seq_no == expected + 1 && held_bytes == 0) {
        held_packet.swap(receive_buffer);
        held_bytes = bytes_received;
        held_seq_no = global_seq_no;
        return bytes_received;
    }

    // Drop duplicate of held packet
    if (held_bytes > 0 && global_seq_no == held_seq_no) {
        return bytes_received;
    }

    // Gap detected - process held packet first if present, then current
    logger.log(LogLevel::Info, "Gap detected: lost %u packet(s) (%u-%u)",
              global_seq_no - expected, expected, global_seq_no - 1);
    if (held_bytes > 0) {
        process_packet_data(held_packet.data(), held_bytes, callback, fromAddress);
        held_bytes = 0;
    }
    process_packet_data(receive_buffer.data(), bytes_received, callback, fromAddress);
    last_global_seq_no = global_seq_no;
    return bytes_received;
}

ssize_t Receiver::Impl::process_packet_data(const uint8_t* packet_data, ssize_t bytes_received, const Callback& callback, const osiSockAddr& fromAddress) {

    Serializer s(const_cast<uint8_t*>(packet_data), (std::size_t)bytes_received);

#if 0
    if (logger.is_loggable(LogLevel::Trace)) {
        std::cerr << s;
    }
#endif

    // Extract global sequence number from header for testing
    if (s.ensure(Header::size)) {
        Header header;
        s >> header;
        //logger.log(LogLevel::Debug, "Processing packet with sequence %u", header.global_seq_no);
    }

    while (s.ensure(SubmessageHeader::size)) {

        SubmessageHeader subheader;
        s >> subheader;

        if ((subheader.flags & SubmessageFlag::LittleEndian) == 0) {
            logger.log(LogLevel::Warning, "Only little endian ordering supported, dropping entire packet from '%s'.",
                        to_string(fromAddress).c_str());
            return bytes_received;
        }

        auto payload_pos = s.position();

        if (subheader.id == SubmessageType::CA_DATA_MESSAGE) {
            if (s.ensure(CADataMessage::size)) {
                CADataMessage data_msg;
                s >> data_msg;

                // Global packet ordering is now handled at header level, so process message directly
                {
                    for (uint16_t i = 0; i < data_msg.channel_count; i++) {
                        if (s.ensure(CAChannelData::size)) {
                            CAChannelData channel_data;
                            s >> channel_data;

                            bool disconnected = (channel_data.count == (uint16_t)-1);

                            if (channel_data.id < channels.size()) {

                                // update last update time
                                Channel& channel = channels[channel_data.id];
                                channel.disconnected = disconnected;
                                channel.last_update_time = current_update_time;

                                // guarded callback call
                                try {
                                    current_processing_seq_no = data_msg.seq_no;

                                    uint32_t count = disconnected ? (uint32_t)-1 : channel_data.count;
                                    callback(channel_data.id, channel_data.type, count, s.position());
                                } catch (std::exception& ex) {
                                    logger.log(LogLevel::Error, "Exception escaped out of callback: %s", ex.what());
                                }
                            }

                            // skip data
                            if (!disconnected) {
                                auto value_size = (std::size_t)dbr_size_n(channel_data.type, channel_data.count);  // parasoft-suppress HICPP-1_2_1-i "Avoid conditions that always evaluate to the same value" - dbr_size_n internal check
                                s += value_size;
                            }

                            s.pos_align(SubmessageHeader::alignment, 0);
                        }
                    }
                }
            }
        }
        else if (subheader.id == SubmessageType::CA_FRAG_DATA_MESSAGE) {
            if (s.ensure(CAFragDataMessage::size)) {
                CAFragDataMessage data_msg;
                s >> data_msg;

                // Global packet ordering is handled at header level, but we still need fragment sequence validation
                if (validate_fragment_sequence(data_msg.seq_no, data_msg.fragment_seq_no)) {

                    // validate channel_id before using it
                    if (data_msg.channel_id < channels.size()) {

                        // first fragment, initialize fragment buffer
                        if (data_msg.fragment_seq_no == 0) {
                            auto total_value_size = (std::size_t)dbr_size_n(data_msg.type, data_msg.count);  // parasoft-suppress HICPP-1_2_1-i "Avoid conditions that always evaluate to the same value" - dbr_size_n internal check
                            fragment_buffer.resize(total_value_size);
                            fragment_serializer = Serializer(fragment_buffer.data(), total_value_size);

                            logger.log(LogLevel::Debug, "Expecting to receive %zu total bytes of fragments for '%s'.",
                                        total_value_size, channels[data_msg.channel_id].name.c_str());
                        }

                        // copy fragment data
                        if (fragment_serializer.remaining() >= data_msg.fragment_size) {
                            fragment_serializer.write(s.position(), data_msg.fragment_size);

                            logger.log(LogLevel::Trace, "Received fragment %u (%zu bytes remaining).",
                                        data_msg.fragment_seq_no, fragment_serializer.remaining());

                            // last fragment received
                            if (fragment_serializer.remaining() == 0) {
                                // update channel metadata
                                channels[data_msg.channel_id].last_update_time = current_update_time;

                                // guarded callback call
                                try {
                                    current_processing_seq_no = data_msg.seq_no;
                                    callback(data_msg.channel_id, data_msg.type, data_msg.count, fragment_buffer.data());
                                } catch (std::exception& ex) {
                                    logger.log(LogLevel::Error, "Exception escaped out of callback: %s", ex.what());
                                }
                            }

                        } else {
                            logger.log(LogLevel::Debug, "Total fragment size out of bounds.");
                        }
                    }

                    // not needed, will be handled below
                    // s += fragment_size;
                }
            }
        }

        if (subheader.bytes_to_next_header == 0) {
            // submessage expands until end of message
            break;
        } else {
            // adjust submessage
            if (s.try_position(payload_pos + subheader.bytes_to_next_header)) {
                // invalid submessage size, dropping packet
                logger.log(LogLevel::Warning, "Submessage 'bytes_to_next_header' out of bounds, received from '%s'.",
                            to_string(fromAddress).c_str());
                break;
            }
        }
    }

    return bytes_received;
}



Receiver::Receiver(const epics_diode::Config& config, int port, std::string listening_address) :
    impl(new Impl(config, port, listening_address)) {
}

Receiver::~Receiver() = default;

void Receiver::run(double runtime, Callback callback) {
    impl->run(runtime, std::move(callback));
}

uint16_t Receiver::get_current_seq_no() const {
    return impl->get_current_seq_no();
}

}

