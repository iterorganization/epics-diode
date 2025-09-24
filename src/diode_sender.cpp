/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsVersion.h>

#include <epics-diode/config.h>
#include <epics-diode/logger.h>
#include <epics-diode/sender.h>
#include <epics-diode/transport.h>
#include <epics-diode/version.h>
#include <epics-diode/utils.h>

namespace edi = epics_diode;

namespace {

char const* const EXECNAME("diode_sender");

void usage()
{
    std::cerr << "\nUsage: " << EXECNAME << " [options] <send address[:port]>...\n"
              << "\n"
              << "options:\n"
              << "  -h            : Help: Print this message\n"
              << "  -V            : Print version and exit\n"
              << "  -d            : Enable debug output\n"
              << "  -r <seconds>  : Runtime in seconds, defaults to forever\n"
              << "  -c <filename> : Set configuration filename, defaults to '" << edi::EPICS_DIODE_CONFIG_FILENAME << "'\n"
              << "\n"
              << "example: " << EXECNAME << " 192.168.12.8:" << edi::EPICS_DIODE_DEFAULT_PORT << "\n"
              << std::endl;
}

}


int main (int argc, char *argv[])
{
    // Configure stdout buffering.
    LINE_BUFFER(stdout);
    
    try {
        int debug_level = 0;
        double runtime = 0.0; // Defaults to forever.
        std::string config_filename = edi::EPICS_DIODE_CONFIG_FILENAME;

        int opt;
        while ((opt = getopt(argc, argv, ":hVdr:c:")) != -1) {
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
            case 'r':
            {
                double temp;
                if ((epicsScanDouble(optarg, &temp)) != 1) {
                    std::cerr << "'" << optarg << "' is not a valid duration value - ignored. ('" << EXECNAME << " -h' for help.)" << std::endl;
                } else {
                    runtime = temp;
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

        // Read send address, only one remaining argument is expected.
        if ((argc - optind) != 1)
        {
            std::cerr << "No or more than one send address specified. ('" << EXECNAME << " -h' for help.)" << std::endl;
            return 1;
        }

        // Set log level.
        edi::Logger::set_default_log_level(edi::LogLevel::from_verbosity(debug_level));

        // Read configuration file.
        auto config = edi::get_configuration(config_filename);

        // Initialize socket subsystem.
        edi::SocketContext socketContext;

        // Run sender.
        std::string send_address = argv[optind];
        edi::Sender(config, send_address).run(runtime);

        return 0;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
