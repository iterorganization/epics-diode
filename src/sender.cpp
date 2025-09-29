/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <cadef.h>
#include <epicsString.h>

#include <epics-diode/config.h>
#include <epics-diode/logger.h>
#include <epics-diode/protocol.h>
#include <epics-diode/sender.h>
#include <epics-diode/transport.h>
#include <epics-diode/version.h>
#include <epics-diode/utils.h>

namespace epics_diode {

struct Channel
{
    uint32_t index = 0;
    uint32_t parent_index = 0;        // parent (=channel) index number
    bool is_polled = false;
    chid channel_id = NULL;
    chtype channel_type = TYPENOTCONN;
    evid event_id = NULL;

    int status = ECA_DISCONN;
    chtype type = TYPENOTCONN;
    long count = -1;
    std::vector<uint8_t> value;
    bool value_hash_initialized = false;
    uint64_t value_hash = 0;
    bool pending_update = false;
    int updates_since_last_hb = 0;

    std::deque<std::uint32_t> &update_deque;
    Channel& parent_channel;

    Channel(uint32_t index,
            uint32_t parent_index,
            bool is_polled,
            std::deque<std::uint32_t> &update_deque,
            std::vector<Channel>& channels) :
        index(index),
        parent_index(parent_index),
        is_polled(is_polled),
        update_deque(update_deque),
        parent_channel((index == parent_index) ? *this : channels[parent_index])
    {
    }

    ~Channel() {
        if (channel_id) {
            ca_clear_channel(channel_id);
        }
    }

    Channel(const Channel&) = delete;
    Channel(Channel&&) = default;

    Channel& operator=(const Channel&) = delete;
    Channel& operator=(Channel&& other) = delete;


    inline bool is_channel() const {
        return index == parent_index;
    }

    inline bool is_field() const {
        return !is_channel();
    }

    inline void mark_update() {
        if (is_field()) {
            parent_channel.mark_update();
            return;
        }
        if (!pending_update) {
            pending_update = true;
            updates_since_last_hb++;
            update_deque.push_back(parent_index);  // Add channel (not field) to update queue 
       }
    }

    bool mark_heartbeat_update() {
        if (is_field()) {
            return parent_channel.mark_heartbeat_update();
        }
        bool to_mark = (updates_since_last_hb == 0);
        if (to_mark) {
            mark_update();
        }
        updates_since_last_hb = 0;
        return to_mark;
    }

    // Assumes 'channel' is on the front of the update_deque.
    void clear_update() {
        if (is_field()) {
            parent_channel.clear_update();
            return;
        }
        update_deque.pop_front();
        pending_update = false;
    }

};

struct ChannelGroup 
{
    const std::vector<Channel>& channels;
    const uint32_t start_index;
    const uint32_t end_index;

    inline uint32_t start_channel_group_id(const Channel& channel) const {
        return channel.parent_index;
    }

    inline uint32_t end_channel_group_id(const Channel& channel) const {
        for (auto i = channel.index; i < channels.size()-1; i++) {
            if (channels[i].parent_index < channels[i+1].parent_index) {
                return i;
            }
        }
        return channels.size() - 1;
    }

    ChannelGroup(const Channel& channel, const std::vector<Channel>& channels) :
        channels(channels),
        start_index(start_channel_group_id(channel)),
        end_index(end_channel_group_id(channel))
    {
    }

    inline uint32_t value_size() const {
        uint32_t sum = 0;
        for (auto i = start_index; i < end_index+1; i++) {
            sum += channels[i].value.size();
        }
        return sum;
    }

    inline uint32_t value_size_aligned(std::size_t alignment) const {
        uint32_t full_aligned_size = 0;
        for (auto i = start_index; i < end_index+1; i++) {
            full_aligned_size += (CAChannelData::size + channels[i].value.size());
            auto n = full_aligned_size % alignment;
            if (n > 0) {
                full_aligned_size += (alignment - n);
            }
        }
        return full_aligned_size;
    }

    inline uint32_t count() const {
        return (end_index - start_index + 1);
    }
};

struct Sender::Impl {
    Impl(const epics_diode::Config& config, const std::string& send_addresses);
    ~Impl();

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;

    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&& other) = delete;

    void run(double runtime);

private:
    Logger logger;
    
    static constexpr double MIN_UPDATE_PERIOD = 0.025;
    static constexpr double MIN_POLLED_FIELDS_UPDATE_PERIOD = 3.0;
    static constexpr double MIN_HB_PERIOD = 0.1;

    UDPSender initialize_sender(const std::string& send_address_list, const Config& config);
    void create_channel(std::vector<Channel>& channels, const std::string channel_name, uint32_t channel_num, uint32_t channel_parent_num, const bool is_polled);
    std::vector<Channel> create_channels(const Config& config);
    
    void send_updates();
    void send_fragmented_updates();
    void send_fragmented_update(Channel* ch);
    void check_polled_fields();
    void mark_heartbeat_updates();

    inline bool has_updates() {
        return !update_deque.empty();
    }

    Channel* next_channel_update() {
        if (update_deque.empty()) {
            return nullptr;
        }

        uint32_t index = update_deque.front();

        Channel* ch = &channels[index];
        return ch;
    }

    double update_period;
    double polled_fields_update_period;
    double heartbeat_period;

    uint64_t iteration = 0;
    const uint64_t pf_iterations;
    const uint64_t hb_iterations;

    std::vector<Serializer::value_type> send_buffer;  
    UDPSender sender;

    uint16_t seq_no = 0;

    std::deque<std::uint32_t> update_deque{};
    std::vector<Channel> channels;

    friend struct Channel;
};

Sender::Impl::Impl(const epics_diode::Config& config, const std::string& send_addresses) :
    logger("sender"),
    update_period(std::max(config.min_update_period, MIN_UPDATE_PERIOD)),
    polled_fields_update_period(std::max(config.polled_fields_update_period, MIN_POLLED_FIELDS_UPDATE_PERIOD)),
    heartbeat_period(std::max(config.heartbeat_period, MIN_HB_PERIOD)),
    pf_iterations(std::max(uint64_t(1), uint64_t(std::round(polled_fields_update_period / update_period)))),
    hb_iterations(std::max(uint64_t(1), uint64_t(std::round(heartbeat_period / update_period)))),
    send_buffer(MAX_MESSAGE_SIZE),
    sender(initialize_sender(send_addresses, config))
{
    logger.log(LogLevel::Config, "Update period %.3fs, heartbeat period %.1fs.",
                update_period, heartbeat_period);

    // Start up Channel Access.
    logger.log(LogLevel::Info, "Initializing CA.");
    int result = ca_context_create(ca_disable_preemptive_callback);
    if (result != ECA_NORMAL) {
        throw std::runtime_error(std::string("Failed to initialize Channel Access: ") + ca_message(result));
    }

    // Create channels.
    channels = create_channels(config);
}

Sender::Impl::~Impl() {
    // Clear channels first and then shutdown CA.
    channels.clear();
    ca_context_destroy();
}

void Sender::Impl::run(double runtime) {

    // Process CA events forever, or specified amount of time.
    auto iterations = uint64_t(std::round(runtime / update_period));
    while (1) {
        ca_pend_event(update_period);

        ++iteration;

        // check for polled fields
        if (iteration % pf_iterations == 0) {
            check_polled_fields();
        }

        // mark stalled updates to be send
        if (iteration % hb_iterations == 0) {
            mark_heartbeat_updates();
        }

        send_updates();

        // runtime check
        if (runtime > 0 && iteration >= iterations) {
            break;
        }
    }
}

UDPSender Sender::Impl::initialize_sender(const std::string& send_address_list, const Config& config)
{
    assert(send_buffer.size() % SubmessageHeader::alignment == 0);

    auto addresses = parse_socket_address_list(send_address_list, EPICS_DIODE_DEFAULT_PORT);

    logger.log(LogLevel::Trace, "Initializing transport.");
    std::string parsed_list;
    for (auto &address : addresses) {
        if (!parsed_list.empty()) {
            parsed_list += ", ";
        }
        parsed_list += to_string(address);
    }

    logger.log(LogLevel::Info, "Initializing transport, send list: [%s].", parsed_list.c_str());
    logger.log(LogLevel::Config, "Send rate-limit set to %uMB/s.", config.rate_limit_mbs);

    uint64_t startup_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Insert header at start.
    Serializer s(send_buffer);
    s << Header(startup_time, config.hash);

    return UDPSender(std::move(addresses), config.rate_limit_mbs);
}

void Sender::Impl::send_fragmented_update(Channel* ch)
{
    auto fragment = ch->value.data();
    uint16_t all_frags_seq_no = seq_no++;
    uint16_t frag_seq_no = 0;
    std::size_t remaining_frag_size = ch->value.size();

    logger.log(LogLevel::Debug, "Sending fragmented data for channel '%s' (%zu bytes).",
                ca_name(ch->channel_id), remaining_frag_size);

    while (remaining_frag_size) {

        Serializer s(send_buffer);
        s += Header::size; // skip preset header

        // we must always fit headers in the buffer
        s.ensure(SubmessageHeader::size + CAFragDataMessage::size);

        s << SubmessageHeader(
                SubmessageType::CA_FRAG_DATA_MESSAGE,
                SubmessageFlag::LittleEndian,
                0);


        // buffer size is limited and always fits uint16_t
        auto frag_size = (uint16_t)std::min(
            remaining_frag_size, 
            s.remaining() - CAFragDataMessage::size);

        s << CAFragDataMessage(
                all_frags_seq_no,
                frag_seq_no++,
                ch->index, ch->count, ch->type,
                frag_size);

        s.write(fragment, frag_size);
        s.pad_align(SubmessageHeader::alignment, 0);

        fragment += frag_size;
        remaining_frag_size -= frag_size;

        logger.log(LogLevel::Trace, "Sending fragment %u (%zu bytes remaining).",
                    (frag_seq_no - 1), remaining_frag_size);

        sender.send(s.data(), s.distance());
    }
}

void Sender::Impl::send_fragmented_updates()
{
    Channel* ch;
    while ((ch = next_channel_update())) {

        // no need for fragmentation
        if (ch->value.size() <= CAChannelData::max_data_size) {
            break;
        }

        send_fragmented_update(ch);
        ch->clear_update();
    }
}


void Sender::Impl::send_updates()
{
    while (has_updates()) {

        Serializer s(send_buffer);
        s += Header::size; // skip preset header
    
        // we must always fit headers in the buffer
        s.ensure(SubmessageHeader::size + CADataMessage::size);

        s << SubmessageHeader(
                SubmessageType::CA_DATA_MESSAGE,
                SubmessageFlag::LittleEndian,
                0);

        bool process_fragmented = false;

        uint16_t update_count = 0;
        s << CADataMessage(seq_no++, update_count);
        auto update_count_pos = s.position() - sizeof(update_count);

        Channel* ch;
        while ((ch = next_channel_update())) {
            ChannelGroup cg(*ch, channels);

            if (cg.value_size() > CAChannelData::max_data_size) {
                process_fragmented = true;
                break;
            }

            // since total buffer size is multiple of required alignment, 
            // there is no need to add padding to ensure call
            if (s.ensure(cg.value_size_aligned(SubmessageHeader::alignment))) {
                for (auto i = cg.start_index; i < cg.end_index+1; i++) {
                    Channel &cc = channels[i];
                    s << CAChannelData(cc.index, cc.count, cc.type);
                    s.write(cc.value.data(), cc.value.size());
                    s.pad_align(SubmessageHeader::alignment, 0);
                    update_count++;
                }
                ch->clear_update();
            } else {
                break;
            }
        }

        // Update update_count.
        std::size_t bytes_to_send = s.distance();
        s.position(update_count_pos);
        s << update_count;

        logger.log(LogLevel::Debug, "Sending %u update(s).", update_count);

        sender.send(s.data(), bytes_to_send);

        if (process_fragmented) {
            send_fragmented_updates();
        }
    }
}


void Sender::Impl::mark_heartbeat_updates()
{
    logger.log(LogLevel::Debug, "Heartbeat check.");

    std::size_t n_connected = 0;
    std::size_t n_marked = 0;
    for (auto& channel : channels) {
        bool marked = channel.mark_heartbeat_update();

        if (channel.status != ECA_DISCONN) {
            n_connected++;
        }
        if (marked) {
            n_marked++;
        }

    }

    std::size_t percent_connected = (100 * n_connected / channels.size());
    std::size_t percent_stalled = (100 * n_marked / channels.size());
    logger.log(LogLevel::Config, "%zu of %zu (%u%%) connected, %zu (%u%%) without updates in the last heartbeat period.", 
        n_connected, channels.size(), percent_connected,
        n_marked, percent_stalled);

}

namespace {

Logger logger("sender.ca");

void event_handler(evargs args)
{
    auto* ch = static_cast<Channel*>(args.usr);

    if (logger.is_loggable(LogLevel::Debug)) {
        logger.log(LogLevel::Debug, "Channel '%s' [%u] event received, status: %d.",
                    ca_name(ch->channel_id), ch->index, ch->status);
    }

    ch->status = args.status;
    if (args.status == ECA_NORMAL)
    {
        ch->count = args.count;

        unsigned size_to_copy = dbr_size_n(args.type, ch->count);
        bool size_changed = ch->value.size() != size_to_copy;

        ch->value.resize(size_to_copy);
        memcpy(ch->value.data(), args.dbr, size_to_copy);

        if (ch->is_polled) {
            auto hash = value_hash(ch->value.data(), size_to_copy);

            if (ch->value_hash_initialized == false || size_changed || ch->value_hash != hash) {
                ch->value_hash_initialized = true;
                ch->value_hash = hash;
                ch->mark_update();
            }
        } else {
            ch->mark_update();
        }
    }
}

void connection_handler(connection_handler_args args)
{
    auto* ch = static_cast<Channel*>(ca_puser(args.chid));
    if (args.op == CA_OP_CONN_UP) {

        logger.log(LogLevel::Debug, "Channel '%s' [%u] connected.",
                    ca_name(ch->channel_id), ch->index);

        chtype new_type = ca_field_type(ch->channel_id);
        long new_count = ca_element_count(ch->channel_id);

        // Re-subscribe on new type.
        if (ch->event_id && ch->channel_type != new_type) {
            if (!ch->is_polled) {
                ca_clear_subscription(ch->event_id);
            }
            ch->event_id = NULL;
        }

        ch->channel_type = new_type;

        // value only for fields, otherwise DBR_TIME_* for default fields
        long mask;
        if (strchr(ca_name(ch->channel_id), '.')) {
            ch->type = ch->channel_type;
            mask = DBE_VALUE;
        }
        else {
            ch->type = dbf_type_to_DBR_TIME(ch->channel_type);
            mask = DBE_VALUE | DBE_ALARM;
        }

        // Re-allocate, if needed.
        auto new_dbr_size = (std::size_t)dbr_size_n(ch->type, new_count);
        if (ch->value.capacity() < new_dbr_size) {
            ch->value.reserve(new_dbr_size);
        }

        if (!ch->is_polled) {
            if (!ch->event_id) {
                ch->status = ca_create_subscription(ch->type,
                                                     new_count,
                                                     ch->channel_id,
                                                     mask,
                                                     event_handler,
                                                     (void*)ch,
                                                     &ch->event_id);
            }
        } else {
            ch->status = ECA_NORMAL;
        }
    }
    else if (args.op == CA_OP_CONN_DOWN) {
        logger.log(LogLevel::Debug, "Channel '%s' [%u] disconnected.",
                    ca_name(ch->channel_id), ch->index);

        // Create disconnected notification update (count == -1).
        ch->status = ECA_DISCONN;
        ch->count = -1;
        ch->value.resize(0);
        ch->mark_update();
    }
}

}

void Sender::Impl::check_polled_fields()
{
    logger.log(LogLevel::Debug, "Polled fields check.");

    for (auto& channel : channels) {
        if (channel.is_polled) {
            ca_array_get_callback(channel.channel_type, (unsigned long) 0, channel.channel_id, event_handler, &channel.index);
        }
    }
}

void Sender::Impl::create_channel(
            std::vector<Channel>& channels,
            const std::string channel_name,
            uint32_t channel_num,
            uint32_t channel_parent_num,
            const bool is_polled)
{
    logger.log(LogLevel::Debug, "Creating channel: [%d] '%s'.", channel_num, channel_name.c_str());

    // Note: use Channel &channel = emplace_back((uint32_t)n, update_deque) with C++17 
    channels.push_back(Channel(channel_num, channel_parent_num, is_polled, update_deque, channels));
    Channel &channel = channels[channel_num];

    int result = ca_create_channel(channel_name.c_str(),
                                   connection_handler,
                                   &channel,
                                   0,
                                   &channel.channel_id);
    if (result != ECA_NORMAL) {
        logger.log(LogLevel::Error, "CA error %s occurred while trying "
                    "to create channel '%s'.", ca_message(result), channel_name.c_str());
        channel.status = result;
    }
}


std::vector<Channel> Sender::Impl::create_channels(const Config& config)
{
    logger.log(LogLevel::Info, "Creating %u channels.", config.total_channel_count());

    assert(config.total_channel_count() <= std::numeric_limits<uint32_t>::max());

    std::vector<Channel> channels;
    channels.reserve(config.total_channel_count()); 
    uint32_t current_channel_num = 0;

    for (auto &config_channel : config.channels) {
        // Create channel for defaultfield.
        const auto channel_parent_num = current_channel_num;

        create_channel(channels,
                       config_channel.channel_name,
                       current_channel_num++,
                       channel_parent_num,
                       false);

        for (auto &field_name : config_channel.extra_fields) {
            create_channel(channels,
                           config_channel.channel_name + "." + field_name,
                           current_channel_num++,
                           channel_parent_num,
                           false);
        }

        for (auto &field_name : config_channel.polled_fields) {
            create_channel(channels,
                           config_channel.channel_name + "." + field_name,
                           current_channel_num++,
                           channel_parent_num,
                           true);
        }
    }

    return channels;
}



Sender::Sender(const epics_diode::Config& config, const std::string& send_addresses) :
    impl(new Impl(config, send_addresses)) {
}

Sender::~Sender() = default;

void Sender::run(double runtime) {
    impl->run(runtime);
}

}
