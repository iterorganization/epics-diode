/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <yajl_parse.h>

#include <epics-diode/config.h>
#include <epics-diode/logger.h>

namespace epics_diode {

namespace {

Logger config_logger("config");

struct ParserContext {
    uint32_t level = 0;
    std::string current_key;
    Config &config;
    ConfigChannel* current_channel;

    explicit ParserContext(Config &config) :
        config(config),
        current_channel(nullptr)
    {
    }
};

static void parser_log_unknown_node(const ParserContext* context) {
    config_logger.log(LogLevel::Config, "Unknown configuration node: '%s'.", context->current_key.c_str());
}

static int parser_yajl_null(void *ctx)
{
    return 1;
}

static int parser_yajl_boolean(void *ctx, int bval)
{
    return 1;
}

static int parser_yajl_double(void *ctx, double dval)
{
    auto* context = static_cast<ParserContext*>(ctx);
    if (context->level == 1) {
        if (context->current_key == "min_update_period") {
            context->config.min_update_period = dval;
        } else if (context->current_key == "polled_fields_update_period") {
            context->config.polled_fields_update_period = dval;
        } else if (context->current_key == "heartbeat_period") {
            context->config.heartbeat_period = dval;
        } else if (context->current_key == "rate_limit_mbs") {
            context->config.rate_limit_mbs = dval;
        }
    }
    return 1;
}

static int parser_yajl_integer(void *ctx, long long ival)
{
    return parser_yajl_double(ctx, ival);
}

static int parser_yajl_string(void *ctx, const unsigned char * sval, size_t len)
{
    auto* context = static_cast<ParserContext*>(ctx);
    if (context->level == 3) {
        std::string value = std::string(reinterpret_cast<const char*>(sval), len);
        if (context->current_key == "extra_fields") {
            if (context->current_channel) {
                context->current_channel->extra_fields.push_back(value);
            }
        } else if (context->current_key == "polled_fields") {
            if (context->current_channel) {
                context->current_channel->polled_fields.push_back(value);
            }
        }
    }
    return 1;
}

static int parser_yajl_map_key(void *ctx, const unsigned char * sval, size_t len)
{
    auto* context = static_cast<ParserContext*>(ctx);
    context->current_key = std::string(reinterpret_cast<const char*>(sval), len);
    if (context->level == 1) {
        if (!(context->current_key == "min_update_period" ||
              context->current_key == "polled_fields_update_period" ||
              context->current_key == "heartbeat_period" ||
              context->current_key == "rate_limit_mbs" ||
              context->current_key == "channel_names")) {
            parser_log_unknown_node(context);
        }
    } else if (context->level == 2) {
        context->config.channels.emplace_back(context->current_key);
        context->current_channel = &(context->config.channels.back());
    }
    // we do not want to do warning logs on nodes on deeper levels,
    // since we have already logs parent node
    return 1;
}

static int parser_yajl_start_map(void *ctx)
{
    auto* context = static_cast<ParserContext*>(ctx);
    context->level++;
    return 1;
}


static int parser_yajl_end_map(void *ctx)
{
    auto* context = static_cast<ParserContext*>(ctx);
    context->level--;
    return 1;
}

static int parser_yajl_start_array(void *ctx)
{
    return 1;
}

static int parser_yajl_end_array(void *ctx)
{
    return 1;
}

static yajl_callbacks parser_callbacks = {
    parser_yajl_null,
    parser_yajl_boolean,
    parser_yajl_integer,
    parser_yajl_double,
    NULL,
    parser_yajl_string,
    parser_yajl_start_map,
    parser_yajl_map_key,
    parser_yajl_end_map,
    parser_yajl_start_array,
    parser_yajl_end_array
};

void parse_json_file(const std::string& filename, Config &config) {

    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    // do a dummy read (peek) to check if file is actually a directory (or empty) 
    file.peek();
    if (!file) {
        throw std::runtime_error("not a valid configuration file: " + filename);
    }

    ParserContext context(config);
    yajl_handle handle = yajl_alloc(&parser_callbacks, NULL, &context);
    if (!handle) {
        throw std::runtime_error("failed to allocate parser");
    } 

    yajl_config(handle, yajl_allow_json5, 1);
    
    yajl_status status = yajl_status_ok;

    std::array<char, 64 * 1024> buffer{};
    auto *parse_buffer = reinterpret_cast<unsigned char *>(buffer.begin());
    std::streamsize bytes_read = 0;
    while (file && status == yajl_status_ok)
    {
        file.read(buffer.begin(), buffer.size());
        bytes_read = file.gcount();
        if (bytes_read > 0) {
            status = yajl_parse(handle, parse_buffer, bytes_read);
       }
    }

    std::string error_msg;
    if (status != yajl_status_ok)
    {
        unsigned char *str = yajl_get_error(handle, 0, parse_buffer, bytes_read);
        error_msg = reinterpret_cast<char*>(str);
        yajl_free_error(handle, str);
    }

    yajl_free(handle);

    if (!error_msg.empty()) {
        throw std::runtime_error("failed to parse: " + error_msg);
    }
}

}

Config get_configuration(const std::string& filename) {
    config_logger.log(LogLevel::Info, "Loading configuration from '%s'.", filename.c_str());

    Config config;
    parse_json_file(filename, config);

    // update hash after loading configuration
    config.update_hash();

    return config;
}

std::ostream& operator<<(std::ostream& os, const Config& c)
{
    for (auto &channel : c.channels) {
        os << "channel '" << channel.channel_name << "' - fields (" << channel.extra_fields.size() << "): ";
        for (auto &field_name : channel.extra_fields) {
            os << "e:'" << field_name << "' ";
        } 
        for (auto &field_name : channel.polled_fields) {
            os << "p:'" << field_name << "' ";
        } 
        os << std::endl;
    }

    return os;
}

}
