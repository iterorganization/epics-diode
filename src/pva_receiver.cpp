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

#include <epics-diode/config.h>
#include <epics-diode/logger.h>
#include <epics-diode/transport.h>
#include <epics-diode/version.h>

#include <epics-diode/pva/protocol.h>
#include <epics-diode/pva/receiver.h>

#include <pvxs/client.h>
#include <pvxs/log.h>
#include <pvxs/util.h>

#include "dataimpl.h"
#include "pvaproto.h"

namespace epics_diode {
namespace pva {

struct Receiver::Impl {
    Impl(const epics_diode::Config& config, int port, std::string listening_address);
    void run(double runtime, Callback callback);

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
        uint16_t last_update_seqno = 0;
        ::pvxs::Value last_value;
        bool disconnected = false;  // we want disconnected event to be sent after start (if not updated within heartbeat period)
        std::chrono::time_point<clock_type> last_update_time{};
    };

    static constexpr std::size_t MAX_PVA_DATA_SIZE = 16 * 1024 * 1024;   

    UDPReceiver initialize_receiver(int port, std::string listening_address, const Config& config);
    std::vector<Channel> create_channels(const Config& config);

    bool validate_order(uint16_t seq_no);
    bool validate_order(uint16_t seq_no, uint16_t fragment_seq_no);
    bool validate_sender(uint64_t startup_time);
    ssize_t receive_updates(const Callback& callback);
    void check_no_updates(Callback callback);

    std::size_t config_hash;
    double heartbeat_period;
    std::vector<Serializer::value_type> receive_buffer;
    std::vector<Serializer::value_type> fragment_buffer;
    Serializer fragment_serializer;

    UDPReceiver receiver;

    uint16_t last_seq_no = (uint16_t)-1;
    uint16_t active_fragment_seq_no = (uint16_t)-1;
    uint16_t last_fragment_seq_no = (uint16_t)-1;
    uint64_t last_startup_time = 0;

    std::vector<Channel> channels;

    TypeCache typeCache;
};

Receiver::Impl::Impl(const Config& config, int port, std::string listening_address) :
    logger("pva.receiver"),
    last_heartbeat_time(clock_type::now()),
    config_hash(config.hash),
    heartbeat_period(config.heartbeat_period),
    receive_buffer(MAX_MESSAGE_SIZE),
    fragment_buffer(MAX_PVA_DATA_SIZE),
    fragment_serializer(fragment_buffer.data(), 0),
    receiver(initialize_receiver(port, listening_address, config)),
    channels(create_channels(config))
{
    // TODO revise
    buildTypeCache(typeCache);
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
                    callback(channel.id, ::pvxs::Value{});
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
        channels.emplace_back(Channel{
                current_channel_num++,
                config_channel.channel_name
        });
    }

    return channels;
}

bool Receiver::Impl::validate_order(uint16_t seq_no) {
    uint16_t diff = seq_no - last_seq_no;

    if (diff != 1 && last_seq_no != (uint16_t)-1) {
        // a bit high logging level, but we want admins to be aware of this
        logger.log(LogLevel::Info, "Packet sequence anomaly detected, %u -> %u!", last_seq_no, seq_no);
    }

    last_seq_no = seq_no;

    constexpr uint16_t tolerable_diff = std::numeric_limits<uint16_t>::max() / 2;
    // not a duplicate (== 0), or tolerable difference (missing sequences)
    // unsigned wraps are handled correctly
    return (/*diff >= 0 && */ diff < tolerable_diff);
}

bool Receiver::Impl::validate_order(uint16_t seq_no, uint16_t fragment_seq_no) {

    // first fragment
    if (fragment_seq_no == 0) {
        
        // check seq_no, remember fragment seq_no
        if (!validate_order(seq_no)) {
            return false;
        } else {
            active_fragment_seq_no = seq_no;
            last_fragment_seq_no = 0;
            return true;
        }

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
        // reset seq_no
        last_seq_no = (uint16_t)-1;
        return true;
    } else {
        // reject older senders
        return false;
    }
}

ssize_t Receiver::Impl::receive_updates(const Callback& callback) {
    osiSockAddr fromAddress;
    auto bytes_received = receiver.receive(receive_buffer.data(), receive_buffer.size(), &fromAddress);
    if (bytes_received <= 0) {
        return bytes_received;
    }

    Serializer s(receive_buffer.data(), (std::size_t)bytes_received);

#if 0
    if (logger.is_loggable(LogLevel::Trace)) {
        std::cerr << s;
    }
#endif

    if (s.ensure(Header::size)) {
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

        if (subheader.id == SubmessageType::PVA_DATA_MESSAGE) {
            if (s.ensure(PVADataMessage::size)) {
                PVADataMessage data_msg;
                s >> data_msg;

                if (validate_order(data_msg.seq_no)) {
                    for (uint16_t i = 0; i < data_msg.channel_count; i++) {
                        if (s.ensure(PVAChannelData::size)) {
                            PVAChannelData channel_data;
                            s >> channel_data;

                            if (channel_data.id < channels.size()) {

                                bool drop_update = false;

                                // update last update time
                                Channel& channel = channels[channel_data.id];
                                channel.disconnected = (channel_data.update_type == Disconnected);

                                if (!channel.disconnected) {
                                    
                                    if (channel_data.update_type == Partial) {
                                        // partial update
                                        // valid only if previous full update was received
                                        // and there was no lost partial update
                                        uint16_t expected_seq_no = channel.last_update_seqno + 1;
                                        if (channel.last_value && expected_seq_no == channel_data.update_seq_no) {
                                            std::cout << "partial update" << std::endl;
                                        } else {
                                            // drop until we get full update with id
                                            std::cout << "partial update, dropping" << std::endl;
                                            drop_update = true;
                                        }
                                    } else {
                                        // full update
                                        std::cout << "full update w/ type id: " << channel_data.type_id << std::endl;
                                        
                                        if (channel_data.type_id < typeCache.size() && typeCache[channel_data.type_id]) {
                                            if (!channel.last_value) {
                                                channel.last_value = typeCache[channel_data.type_id].cloneEmpty();
                                            } else {
                                                // TODO check if type-s are the same!!!
                                            }
                                        } else {
                                            // unknown type ID, drop
                                            // TODO log
                                            std::cout << "unknown type ID: " << channel_data.type_id << std::endl;
                                            drop_update = true;
                                        }
                                    }

                                    if (!drop_update) {
                                        channel.last_value.unmark();
                                        s >> channel.last_value;
                                    }

                                } else {
                                    // invalidate value to notify disconnection
                                    channel.last_value = ::pvxs::Value();
                                    std::cout << "disconnected" << std::endl;
                                }

                                if (!drop_update) {
                                    channel.last_update_seqno = channel_data.update_seq_no;
                                    channel.last_update_time = current_update_time;

                                    // guarded callback call
                                    try {
                                        callback(channel_data.id, channel.last_value);
                                    } catch (std::exception& ex) {
                                        logger.log(LogLevel::Error, "Exception escaped out of callback: %s", ex.what());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (subheader.id == SubmessageType::PVA_TYPEDEF_MESSAGE) {
            if (s.ensure(PVATypeDefMessage::size)) {
                PVATypeDefMessage td_msg;
                s >> td_msg;

                for (uint16_t i = 0; i < td_msg.typedef_count; i++) {
                    ::pvxs::Value type_value;
                    TypeDefSerializer::deserialize(s, type_value); // TODO check

                    // TODO move to TypeCache strct
                    uint16_t index = td_msg.start_id + i;
                    if (index >= typeCache.size()) {
                        typeCache.resize(index + 1);
                    } 

                    // do not override
                    // TODO just check if equal?
                    if (!typeCache[index]) {
                        typeCache[index] = std::move(type_value);
                    }
                }
            }
        }
/*
        else if (subheader.id == SubmessageType::PVA_FRAG_DATA_MESSAGE) {
            if (s.ensure(PVAFragDataMessage::size)) {
                PVAFragDataMessage data_msg;
                s >> data_msg;

                if (validate_order(data_msg.seq_no, data_msg.fragment_seq_no)) {

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
                            // guarded callback call
                            try {
                                callback(data_msg.channel_id, data_msg.type, data_msg.count, fragment_buffer.data());
                            } catch (std::exception& ex) {
                                logger.log(LogLevel::Error, "Exception escaped out of callback: %s", ex.what());
                            }
                        }

                    } else {
                        logger.log(LogLevel::Debug, "Total fragment size out of bounds.");
                    }

                    // not needed, will be handled below
                    // s += fragment_size;
                }
            }
        }
        */

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

}
}
