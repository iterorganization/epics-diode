/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_CONFIG_H
#define EPICS_DIODE_CONFIG_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <ostream>

namespace epics_diode {

const char* const EPICS_DIODE_CONFIG_FILENAME("diode.json");


struct ConfigChannel {
    std::string channel_name;
    std::vector<std::string> extra_fields;
    std::vector<std::string> polled_fields;

    ConfigChannel() {
    }

    explicit ConfigChannel(std::string channel_name)
        : channel_name(channel_name)
    {
    }
};

struct Config {
    std::size_t hash = 0;                      // 0 indicates "do not check"
    double min_update_period = 0.1;            // 0.1s
    double polled_fields_update_period = 5.0;  // 5.0s
    double heartbeat_period = 15.0;            // 15s
    uint32_t rate_limit_mbs = 64;              // 64Mb/s, suitable for 1Gb network
    std::vector<ConfigChannel> channels;

    void update_hash()
    {
        hash = std::hash<double>{}(min_update_period);
        hash ^= std::hash<double>{}(polled_fields_update_period) << 1;
        hash ^= std::hash<double>{}(heartbeat_period) << 1;
        hash ^= std::hash<uint32_t>{}(rate_limit_mbs) << 1;
        for (auto &channel : channels) {
            hash ^= std::hash<std::string>{}(channel.channel_name) << 1;
            for (auto &field_name : channel.extra_fields) {
                hash ^= std::hash<std::string>{}(field_name) << 1;
            }
            for (auto &field_name : channel.polled_fields) {
                hash ^= std::hash<std::string>{}(field_name) << 1;
            }
        }
    }

    std::size_t total_channel_count() const
    {
        std::size_t result = 0;
        for (auto &channel : channels) {
            result += (channel.extra_fields.size() + channel.polled_fields.size() + 1);
        }
        return result;
    }

    const std::vector<std::string> create_flat_channel_name_vector() const
    {
        std::vector<std::string> flat_channel_name_vector;
        flat_channel_name_vector.reserve(total_channel_count());

        for (auto &channel : channels) {
            flat_channel_name_vector.push_back(channel.channel_name);

            for (auto &field_name : channel.extra_fields) {
                flat_channel_name_vector.push_back(channel.channel_name + "." + field_name);
            }
            for (auto &field_name : channel.polled_fields) {
                flat_channel_name_vector.push_back(channel.channel_name + "." + field_name);
            }
        }
        return flat_channel_name_vector;
    }

    friend std::ostream& operator<<(std::ostream& os, const Config& c);
};



Config get_configuration(const std::string& filename);

}

#endif
