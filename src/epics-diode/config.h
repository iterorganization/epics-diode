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

namespace {

uint64_t fnv1a_hash(const void* data, size_t size,
                    uint64_t seed = 1469598103934665603ULL) {
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  uint64_t hash = seed;
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

inline uint64_t hash_combine(uint64_t h1, uint64_t h2) {
  return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
}

uint64_t hash_double(double v) { return fnv1a_hash(&v, sizeof(v)); }

uint64_t hash_uint32(uint32_t v) { return fnv1a_hash(&v, sizeof(v)); }

uint64_t hash_string(const std::string& s) {
  return fnv1a_hash(s.data(), s.size());
}

}  // namespace

struct Config {
    std::size_t hash = 0;                      // 0 indicates "do not check"
    double min_update_period = 0.1;            // 0.1s
    double polled_fields_update_period = 5.0;  // 5.0s
    double heartbeat_period = 15.0;            // 15s
    uint32_t rate_limit_mbs = 64;              // 64Mb/s, suitable for 1Gb network
    std::vector<ConfigChannel> channels;

    void update_hash()
    {
        hash = 1469598103934665603ULL;  // FNV offset basis

        hash = hash_combine(hash, hash_double(min_update_period));
        hash = hash_combine(hash, hash_double(polled_fields_update_period));
        hash = hash_combine(hash, hash_double(heartbeat_period));
        hash = hash_combine(hash, hash_uint32(rate_limit_mbs));

        for (auto &channel : channels) {
            hash = hash_combine(hash, hash_string(channel.channel_name));
            for (auto &field_name : channel.extra_fields) {
                hash = hash_combine(hash, hash_string(field_name));
            }
            for (auto &field_name : channel.polled_fields) {
                hash = hash_combine(hash, hash_string(field_name));
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
