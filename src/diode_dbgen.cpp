/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <type_traits>

#include <cadef.h>

#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsVersion.h>

#include <epics-diode/version.h>
#include <epics-diode/logger.h>
#include <epics-diode/config.h>
#include <epics-diode/utils.h>

namespace edi = epics_diode;

namespace {

char const* const EXECNAME("diode_dbgen");
double const DEFAULT_TIMEOUT(5.0);  // parasoft-suppress HICPP-7_1_6-b HICPP-3_5_1-b "Intentional use of double!"

void usage()
{
    std::cerr << "\nUsage: " << EXECNAME << " [options] [output filename]...\n"
              << "\n"
              << "options:\n"
              << "  -h            : Help: Print this message\n"
              << "  -V            : Print version and exit\n"
              << "  -d            : Enable debug output\n"
              << "  -w <seconds>  : Wait time, specifies CA timeout, defaults to " << TO_STRING(DEFAULT_TIMEOUT) << " seconds\n"
              << "  -c <filename> : Set configuration filename, defaults to '" << edi::EPICS_DIODE_CONFIG_FILENAME << "'\n"
              << "\n"
              << "example: " << EXECNAME << "\n"
              << std::endl;
}

}

template <typename DbrType>
void generate_units_and_limits(std::ostream& s, DbrType* dbr) {
    if (strlen(dbr->units)) {
        s << "  field(EGU,  \"" << dbr->units << "\")\n";
    }
    //s << "  field(???, \"" << dbr->upper_disp_limit << "\")\n"; // == HOPR
    //s << "  field(???, \"" << dbr->lower_disp_limit << "\")\n"; // == LOPR
    if (!std::isnan(dbr->upper_alarm_limit)) {
        s << "  field(HIHI, \"" << dbr->upper_alarm_limit << "\")\n";
    }
    if (!std::isnan(dbr->upper_warning_limit)) {
        s << "  field(HIGH, \"" << dbr->upper_warning_limit << "\")\n";
    }
    if (!std::isnan(dbr->lower_warning_limit)) {
        s << "  field(LOW,  \"" << dbr->lower_warning_limit << "\")\n";
    }
    if (!std::isnan(dbr->lower_alarm_limit)) {
        s << "  field(LOLO, \"" << dbr->lower_alarm_limit << "\")\n";
    }
    if (!(dbr->upper_ctrl_limit == 0 && dbr->lower_ctrl_limit == 0)) {
        s << "  field(HOPR, \"" << dbr->upper_ctrl_limit << "\")\n";
        s << "  field(LOPR, \"" << dbr->lower_ctrl_limit << "\")\n";
    }
}

template <typename DbrType>
void generate_ctrl(std::ostream& s, DbrType* dbr) {
    generate_units_and_limits(s, dbr);
}

template <typename DbrType, typename std::enable_if<std::is_floating_point<typename DbrType::value>::value>::type = true>
void generate_ctrl(std::ostream& s, dbr_ctrl_float* dbr) {
    if (dbr->precision) {
        s << "  field(PREC, \"" << dbr->precision << "\")\n";
    }
    generate_units_and_limits(s, dbr);
}

template <>
void generate_ctrl(std::ostream& s, dbr_sts_string* dbr) {
    // dbr_sts_string has no other fields
}

template <typename DbrType>
void generate_record_from_ctrl(std::ostream& s, 
                               uint32_t channel_index, const char* channel_name, const std::string& record_type,
                               chtype type, uint32_t count, DbrType* dbr) {
    s << "record(" << record_type << ", \"" << channel_name << "\")\n{\n"; 
    // index
    s << "  info(diode_cix, \"" << channel_index << "\")\n";
    // wf specific
    if (record_type == "waveform" ||
        record_type == "aai" ||
        record_type == "aao" || 
        record_type == "subArray") {
        s << "  field(FTVL, \"" << (dbr_text[type] + 4) << "\")\n";
        s << "  field(NELM, \"" << count << "\")\n";
    } else if (record_type == "compress") {
        s << "  field(NSAM, \"" << count << "\")\n";
    }
    // CTRL
    generate_ctrl(s, dbr);
    s << "}\n\n";
}

const std::array<std::string, 16> MBBI_ENUMS = {
    "ZRST",
    "ONST",
    "TWST",
    "THST",
    "FRST",
    "FVST",
    "SXST",
    "SVST",
    "EIST",
    "NIST",
    "TEST",
    "ELST",
    "TVST",
    "TTST",
    "FTST",
    "FFST"
};

// DBR_CTRL_ENUM specialization
template <>
void generate_record_from_ctrl(std::ostream& s, 
                               uint32_t channel_index, const char* channel_name, const std::string& record_type,
                               chtype type, uint32_t count, dbr_ctrl_enum* data) {
    s << "record(" << record_type << ", \"" << channel_name << "\")\n{\n"; 
     // index
    s << "  info(diode_cix, \"" << channel_index << "\")\n";
    for (dbr_short_t n = 0; n < data->no_str; n++) {
        s << "  field(" << MBBI_ENUMS[n] << ",  \"" << data->strs[n] << "\")\n";
    }
    s << "}\n\n";
}

int main (int argc, char *argv[])
{
    // Configure stdout buffering.
    LINE_BUFFER(stdout);
    
    try {
        int debug_level = 0;
        double timeout = 0.0; // Defaults to forever.
        std::string config_filename = edi::EPICS_DIODE_CONFIG_FILENAME;

        int opt;
        while ((opt = getopt(argc, argv, ":hVdw:c:")) != -1) {
            switch (opt) {
            case 'h':
                usage();
                return 0;
            case 'V':
            {
                std::cout << EXECNAME << ' '
                          << EPICS_DIODE_MAJOR_VERSION << '.'
                          << EPICS_DIODE_MINOR_VERSION << '.'
                          << EPICS_DIODE_MAINTENANCE_VERSION
                          << ((EPICS_DIODE_DEVELOPMENT_FLAG) ? "-SNAPSHOT" : "") << std::endl;
                std::cout << "Base " << EPICS_VERSION_FULL << std::endl;
                return 0;
            }
            case 'd':
                debug_level++;
                break;
            case 'w':
            {
                double temp;
                if ((epicsScanDouble(optarg, &temp)) != 1) {
                    std::cerr << "'" << optarg << "' is not a valid timeout value - ignored. ('" << EXECNAME << " -h' for help.)" << std::endl;
                } else {
                    timeout = temp;
                }
                break;
            }
            case 'c':
                config_filename = optarg;
                break;
            case '?':
                std::cerr << "Unrecognized option: '" << (char)optopt << "'. ('" << EXECNAME << " -h' for help.)" << std::endl;
                return 1;
            case ':':
                std::cerr << "Option '" << (char)optopt << "' requires an argument. ('" << EXECNAME << " -h' for help.)" << std::endl;
                return 1;
            default :
                usage();
                return 1;
            }
        }

        std::unique_ptr<std::ofstream> file_out;
        if ((argc - optind) == 1)
        {
             //file_out = std::make_unique<std::ofstream>(argv[optind]); 
             file_out = std::unique_ptr<std::ofstream>(new std::ofstream(argv[optind])); 
        } else if ((argc - optind) > 1) {
            std::cerr << "More than one port specified. ('" << EXECNAME << " -h' for help.)" << std::endl;
            return 1;
        }
        std::ostream& s = file_out.get() ? *file_out.get() : std::cout;

        // put a header to the file only
        if (file_out.get()) {
            s << "# generated by " << EXECNAME << std::endl << std::endl;
        }

        // Set log level.
        edi::Logger::set_default_log_level(edi::LogLevel::from_verbosity(debug_level));

        edi::Logger logger(EXECNAME);

        // Read configuration file.
        auto config = edi::get_configuration(config_filename);

        // Start up Channel Access.
        logger.log(edi::LogLevel::Info, "Initializing CA.");
        int result = ca_context_create(ca_disable_preemptive_callback);
        if (result != ECA_NORMAL) {
            throw std::runtime_error(std::string("Failed to initialize Channel Access: ") + ca_message(result));
        }

        // NOTE: the following code is sequential, parallel/async version would be far more efficient.

        uint32_t channel_index = 0;
        for (auto &config_channel : config.channels) {
            logger.log(edi::LogLevel::Info, "Processing %u/%zu: '%s'.", 
                channel_index + 1, config.channels.size(), config_channel.channel_name.c_str());

            chid channel_id;
            char data[sizeof(db_access_val)];
            result = ca_create_channel(config_channel.channel_name.c_str(),
                                        0, 0, 0,
                                        &channel_id);
            ca_pend_io(timeout);

            // get record type
            ca_array_get(DBR_CLASS_NAME, 1, channel_id, (void *)data);
            ca_pend_io(timeout);
            std::string record_type(*((dbr_string_t*)dbr_value_ptr(data, DBR_CLASS_NAME)));

            // we do no care about value, so get only one element
            chtype type = ca_field_type(channel_id);
            unsigned count = ca_element_count(channel_id);
            chtype get_type = dbf_type_to_DBR_CTRL(type);
            unsigned get_count = 1;
            ca_array_get(get_type, get_count, channel_id, (void *)data);
            ca_pend_io(timeout);


#define HANDLE_TYPE(dbr_type) \
                generate_record_from_ctrl(s, \
                    channel_index, ca_name(channel_id), record_type, \
                    type, count, (dbr_type*)data)

            if (get_type == DBR_CTRL_STRING)
                HANDLE_TYPE(dbr_sts_string);
            else if (get_type == DBR_CTRL_SHORT)
                HANDLE_TYPE(dbr_ctrl_short);
            else if (get_type == DBR_CTRL_INT)
                HANDLE_TYPE(dbr_ctrl_int);
            else if (get_type == DBR_CTRL_FLOAT)
                HANDLE_TYPE(dbr_ctrl_float);
            else if (get_type == DBR_CTRL_ENUM)
                HANDLE_TYPE(dbr_ctrl_enum);
            else if (get_type == DBR_CTRL_CHAR)
                HANDLE_TYPE(dbr_ctrl_char);
            else if (get_type == DBR_CTRL_LONG)
                HANDLE_TYPE(dbr_ctrl_long);
            else if (get_type == DBR_CTRL_DOUBLE)
                HANDLE_TYPE(dbr_ctrl_double);

#undef HANDLE_TYPE

            if (result != ECA_NORMAL) {
                logger.log(edi::LogLevel::Error, "CA error %s occurred while trying "
                            "to create channel '%s'.\n", ca_message(result), config_channel.channel_name.c_str());
            }

            channel_index += (config_channel.extra_fields.size() + config_channel.polled_fields.size() + 1);
        }


        return 0;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
