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

#include <thread>
#include <chrono>

#include <epicsString.h>

#include <epics-diode/config.h>
#include <epics-diode/logger.h>
#include <epics-diode/transport.h>
#include <epics-diode/version.h>
#include <epics-diode/utils.h>

#include <epics-diode/pva/protocol.h>
#include <epics-diode/pva/sender.h>

#include <pvxs/client.h>
#include <pvxs/log.h>
#include <pvxs/util.h>

#include "bitmask.h"
#include "dataimpl.h"

namespace epics_diode {
namespace pva {

static void valid_to_mask(::pvxs::Value& src_value, ::pvxs::BitMask& mask)
{
    auto desc = ::pvxs::Value::Helper::desc(src_value);
    auto src_store = ::pvxs::Value::Helper::store_ptr(src_value);
    assert(desc && desc->code==::pvxs::TypeCode::Struct);
    assert(mask.size()==desc->size());  // parasoft-suppress HICPP-5_2_1-c "desc cannot be null and it is checked line above."

    for (size_t bit = 0u, N = desc->size(); bit < N; bit++) {
         mask[bit] = src_store[bit].valid;
    }
}

static void merge(::pvxs::Value& dest_value, ::pvxs::BitMask& mask, ::pvxs::Value& src_value)
{
    auto desc = ::pvxs::Value::Helper::desc(src_value);
    auto src_store = ::pvxs::Value::Helper::store_ptr(src_value);
    auto dest_store = ::pvxs::Value::Helper::store_ptr(dest_value);
    assert(desc && desc->code==::pvxs::TypeCode::Struct);
    assert(dest_value.equalType(src_value)); // TODO is is heavy"!
    assert(mask.size()==desc->size());  // parasoft-suppress HICPP-5_2_1-c "desc cannot be null and it is checked line above."

    for (size_t bit = 0u, N = desc->size(); bit<N;) {

        auto &src = src_store[bit];
        auto &dst = dest_store[bit];

        if(src.valid) {
            
            switch(dst.code) {
            case ::pvxs::StoreType::Null:
                break;
            case ::pvxs::StoreType::Bool:
            case ::pvxs::StoreType::UInteger:
            case ::pvxs::StoreType::Integer:
            case ::pvxs::StoreType::Real:
                memcpy(&dst.store, &src.store, sizeof(src.store));
                break;
            case ::pvxs::StoreType::String:
                dst.as<std::string>() = src.as<std::string>();
                break;
            case ::pvxs::StoreType::Array:
                dst.as<::pvxs::shared_array<const void>>() = src.as<::pvxs::shared_array<const void>>();
                break;
            case ::pvxs::StoreType::Compound:
            {
                std::shared_ptr<::pvxs::impl::FieldStorage> sstore(::pvxs::Value::Helper::store(src_value), &src);
                auto& dfld(dst.as<::pvxs::Value>());
                ::pvxs::Value::Helper::set_desc(dfld, &desc[bit]);
                ::pvxs::Value::Helper::store(dfld) = std::move(sstore);
            }
                break;
            }

            mask[bit] = true;
            bit += desc[bit].size(); // maybe skip past entire sub-struct
        } else {
            bit++;
        }
    }
}


struct Channel
{
    uint32_t index = 0;
    
    bool connected = false;
    uint16_t type_id = 0;
    uint16_t update_seq_no = 0;
    ::pvxs::Value value;
    ::pvxs::BitMask changed_mask;

    UpdateType pending_update = None;
    int updates_since_last_hb = 0;

    std::deque<std::uint32_t> &update_deque;
    std::shared_ptr<::pvxs::client::Subscription> subscription;

    Channel(uint32_t index,
            std::deque<std::uint32_t> &update_deque) :
        index(index),
        update_deque(update_deque)
    {
    }

    ~Channel() {
    }

    Channel(const Channel&) = delete;
    Channel(Channel&&) = default;

    Channel& operator=(const Channel&) = delete;
    Channel& operator=(Channel&& other) = default;

    inline void mark_update(UpdateType kind) {
        if (pending_update == None) {
            pending_update = kind;
            if (kind == Full) {
                valid_to_mask(value, changed_mask);
            }
            updates_since_last_hb++;
            update_deque.push_back(index); 
        }
        // always upgrade to full
        else if (kind == Full && pending_update != Full) {
            pending_update = kind;
            valid_to_mask(value, changed_mask);
        }
    }

    bool mark_heartbeat_update() {
        // always send heartbeats, we need periodic Full updates
        mark_update(Full);
        updates_since_last_hb = 0;
        return true;
    }

    // Assumes 'channel' is on the front of the update_deque.
    void clear_update() {
        update_deque.pop_front();

        // clear all bits
        auto nwords = changed_mask.wsize();
        for (size_t wi = 0; wi < nwords; wi++) {
            changed_mask.word(wi) = 0;
        }

        pending_update = None;
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
    std::vector<Channel> create_channels(const Config& config);
    
    void send_typedef_updates();
    void send_updates();
    void send_fragmented_updates();
    void send_fragmented_update(Channel* ch);
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
    double heartbeat_period;

    uint64_t iteration = 0;
    const uint64_t hb_iterations;

    std::vector<Serializer::value_type> send_buffer;  
    UDPSender sender;

    uint16_t seq_no = 0;

    std::deque<std::uint32_t> update_deque{};
    std::vector<Channel> channels;

    ::pvxs::client::Context context;
    ::pvxs::MPMCFIFO<int> workqueue;

    TypeCache typeCache;

    friend struct Channel;
};

Sender::Impl::Impl(const epics_diode::Config& config, const std::string& send_addresses) :
    logger("pva.sender"),
    update_period(std::max(config.min_update_period, MIN_UPDATE_PERIOD)),
    heartbeat_period(std::max(config.heartbeat_period, MIN_HB_PERIOD)),
    hb_iterations(std::max(uint64_t(1), uint64_t(std::round(heartbeat_period / update_period)))),
    send_buffer(MAX_MESSAGE_SIZE),
    sender(initialize_sender(send_addresses, config))
{
    logger.log(LogLevel::Config, "Update period %.3fs, heartbeat period %.1fs.",
                update_period, heartbeat_period);

    // Start up Channel Access.
    logger.log(LogLevel::Info, "Initializing PVA.");

    // (Optional) configuring logging using $PVXS_LOG
    ::pvxs::logger_config_env();

    // Configure client using $EPICS_PVA_*
    context = ::pvxs::client::Context::fromEnv();

    // Build type cache.
    buildTypeCache(typeCache);

    // Create channels.
    channels = create_channels(config);
}

Sender::Impl::~Impl() {
    // Clear channels first and then close PVA.
    channels.clear();
    context.close();
}

void Sender::Impl::run(double runtime) {
    // Process PVA events forever, or specified amount of time.
    auto iterations = uint64_t(std::round(runtime / update_period));
    while (1)
    {
        // sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(int64_t(update_period * 1000)));

        while (workqueue.size())
        {
            auto index = workqueue.pop();    
            auto &channel = channels[index]; 
            auto &monitor = channel.subscription;
            auto &name = monitor->name();
            while (1)
            {
                try
                {
                    auto value = monitor->pop();
                    if (!value) {
                        break;
                    }

                    if (logger.is_loggable(LogLevel::Debug)) {
                        logger.log(LogLevel::Debug, "Channel '%s' [%u] update received.",
                                    name.c_str(), channel.index);
                    }

                    // update
                    if (!channel.connected) {
                        channel.connected = true;
                        channel.value = std::move(value);
                        channel.type_id = cacheType(typeCache, channel.value);
                        channel.changed_mask.resize(::pvxs::Value::Helper::desc(channel.value)->size());
                        channel.update_seq_no = 0;
                        channel.mark_update(Full);
                    } else {
                        // merge
                        merge(channel.value, channel.changed_mask, value);
                        channel.mark_update(Partial);
                    }
                }
                catch (::pvxs::client::Connected &connection)
                {
                    logger.log(LogLevel::Debug, "Channel '%s' [%u] connected.", name.c_str(), channel.index);

                    // do nothing here, we really need to get an update on connection time
                }
                catch (::pvxs::client::Disconnect &connection)
                {
                    logger.log(LogLevel::Debug, "Channel '%s' [%u] disconnected.",
                                name.c_str(), channel.index);

                    channel.connected = false;
                    channel.mark_update(Partial);
                }
                catch (std::exception &err)
                {
                    std::cerr << name << " error " << typeid(err).name() << " : " << err.what() << std::endl;
                }
            }
        }

        ++iteration;

        // mark heartbeat updates to be send
        if (iteration % hb_iterations == 0) {
            send_typedef_updates();
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

    auto addresses = parse_socket_address_list(send_address_list, EPICS_PVADIODE_DEFAULT_PORT);

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
    std::cerr << "send_fragmented_update not yet implemeted" << std::endl;
/*TODO
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
        s.ensure(SubmessageHeader::size + PVA_FRAG_DATA_MESSAGE::size);

        s << SubmessageHeader(
                SubmessageType::PVA_FRAG_DATA_MESSAGE,
                SubmessageFlag::LittleEndian,
                0);


        // buffer size is limited and always fits uint16_t
        auto frag_size = (uint16_t)std::min(
            remaining_frag_size, 
            s.remaining() - PVAFragDataMessage::size);

        s << PXVSFragDataMessage(
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
*/
}

void Sender::Impl::send_fragmented_updates()
{
    Channel* ch;
    while ((ch = next_channel_update())) {

/*TODO
        // no need for fragmentation
        if (ch->value.size() <= PVAChannelData::max_data_size) {
            break;
        }
*/
        send_fragmented_update(ch);
        ch->clear_update();
    }
}

void Sender::Impl::send_typedef_updates()
{
    uint16_t id = TypeCache_::buildinCacheSize;
    while (size_t(id) < typeCache.size()) {

        Serializer s(send_buffer);
        s += Header::size; // skip preset header
    
        // we must always fit headers in the buffer
        s.ensure(SubmessageHeader::size + PVATypeDefMessage::size);

        s << SubmessageHeader(
                SubmessageType::PVA_TYPEDEF_MESSAGE,
                SubmessageFlag::LittleEndian,
                0);

        uint16_t update_count = 0;
        s << PVATypeDefMessage(id, update_count);
        auto update_count_pos = s.position() - sizeof(update_count);

        while (size_t(id) < typeCache.size()) {
            if (TypeDefSerializer::serialize(s, typeCache[id])) {       // TODO if typedef > max buffer size then is a deadlock
                update_count++;
                id++;
            } else {
                break;
            }
        }

        s.pad_align(SubmessageHeader::alignment, 0);

        // Update update_count.
        std::size_t bytes_to_send = s.distance();
        s.position(update_count_pos);
        s << update_count;

        logger.log(LogLevel::Debug, "Sending %u typedef update(s).", update_count);

        sender.send(s.data(), bytes_to_send);

    }
}

void Sender::Impl::send_updates()
{
    while (has_updates()) {

        Serializer s(send_buffer);
        s += Header::size; // skip preset header
    
        // we must always fit headers in the buffer
        s.ensure(SubmessageHeader::size + PVADataMessage::size);

        s << SubmessageHeader(
                SubmessageType::PVA_DATA_MESSAGE,
                SubmessageFlag::LittleEndian,
                0);

        bool process_fragmented = false;

        uint16_t update_count = 0;
        s << PVADataMessage(seq_no++, update_count);
        auto update_count_pos = s.position() - sizeof(update_count);

        Channel* ch;
        while ((ch = next_channel_update())) {
/*
            if (cg.value_size() > PVAChannelData::max_data_size) {
                process_fragmented = true;
                break;
            }
*/
            PVAChannelData data_msg(ch->index, ch->update_seq_no, ch->pending_update, ch->type_id);
            s << data_msg;
            if (data_msg.serialize(s, ch->value, &ch->changed_mask))    // TODO not the nicest solution
            { 
                ch->update_seq_no++;
                ch->clear_update();
                update_count++;
            } else {
                break;
            }
        }

        s.pad_align(SubmessageHeader::alignment, 0);

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

        if (channel.connected) {
            n_connected++;
        }
        if (marked) {
            n_marked++;
        }

    }

    std::size_t percent_connected = (100 * n_connected / channels.size());
    std::size_t percent_stalled = (100 * n_marked / channels.size());
    logger.log(LogLevel::Config, "%zu of %zu (%u%%) connected, %zu (%u%%) heartbeat updates in the last heartbeat period.", 
        n_connected, channels.size(), percent_connected,
        n_marked, percent_stalled);
}



std::vector<Channel> Sender::Impl::create_channels(const Config& config)
{
    logger.log(LogLevel::Info, "Creating %u channels.", config.total_channel_count());

    assert(config.total_channel_count() <= std::numeric_limits<uint32_t>::max());

    std::vector<Channel> channels;
    channels.reserve(config.total_channel_count()); 
    uint32_t current_channel_num = 0;

    for (auto &config_channel : config.channels) {

        logger.log(LogLevel::Debug, "Creating channel: [%d] '%s'.", current_channel_num, config_channel.channel_name.c_str());

        // Note: use Channel &channel = emplace_back((uint32_t)n, update_deque) with C++17 
        channels.push_back(Channel(current_channel_num, update_deque));
        Channel &channel = channels[current_channel_num];

        // TODO configurable
        std::string request; //("field(value)");

        channel.subscription = context.monitor(config_channel.channel_name)
                        .pvRequest(request)
                        .record("queueSize", 1)
                        .maskConnected(false)
                        .maskDisconnected(false)
                        .event([this, &channel](::pvxs::client::Subscription& monitor)
                        {
                            workqueue.push(channel.index);
                        })
                        .exec();

        current_channel_num++;
    }

    // expedite search after starting all requests
    context.hurryUp();

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
}
