/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

#include <cadef.h>

#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsVersion.h>

#include <epics-diode/config.h>
#include <epics-diode/logger.h>
#include <epics-diode/transport.h>
#include <epics-diode/version.h>
#include <epics-diode/utils.h>

#include <epics-diode/pva/receiver.h>

#include <pvxs/server.h>
#include <pvxs/sharedpv.h>
#include <pvxs/log.h>
#include <pvxs/util.h>

namespace edi = epics_diode;

namespace {

char const* const EXECNAME("pvadiode_receiver");
int const EPICS_SCAN_BASE = 10;  // parasoft-suppress HICPP-7_1_6-b HICPP-3_5_1-b "Usage of basic type intentional: Library use!"

void usage()
{
    std::cerr << "\nUsage: " << EXECNAME << " [options] [<receive port>]...\n"
              << "\n"
              << "options:\n"
              << "  -h            : Help: Print this message\n"
              << "  -V            : Print version and exit\n"
              << "  -d            : Enable debug output\n"
              << "  -r <seconds>  : Runtime in seconds, defaults to forever\n"
              << "  -c <filename> : Set configuration filename, defaults to '" << edi::EPICS_DIODE_CONFIG_FILENAME << "'\n"
              << "  -i <address>  : Only listen on specified address, defaults listens on all addresses.'\n"
              << "\n"
              << "example: " << EXECNAME << "\n"
              << std::endl;
}

}


int main (int argc, char *argv[])
{
    // Configure stdout buffering.
    LINE_BUFFER(stdout);
    
    try {
        int port = edi::EPICS_PVADIODE_DEFAULT_PORT;
        int debug_level = 0;
        double runtime = 0.0; // Defaults to forever.
        std::string config_filename = edi::EPICS_DIODE_CONFIG_FILENAME;
        std::string listening_address = edi::EPICS_DIODE_DEFAULT_LISTENING_ADDRESS;

        int opt;
        while ((opt = getopt(argc, argv, ":hVdr:c:i:")) != -1) {
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
                //std::cout << pvxs::version_information;
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
            case 'i':
                listening_address = optarg;
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

        // Read bind port, only one remaining argument is expected.
        if ((argc - optind) == 1)
        {
            unsigned long temp;
            if ((epicsScanULong(argv[optind], &temp, 10)) != 1) {
                std::cerr << "'" << argv[optind] << "' is not a valid port value - ignored. ('" << EXECNAME << " -h' for help.)" << std::endl;
            } else {
                port = temp;
            }
        } else if ((argc - optind) > 1) {
            std::cerr << "More than one port specified. ('" << EXECNAME << " -h' for help.)" << std::endl;
            return 1;
        }

        // Set log level.
        edi::Logger::set_default_log_level(edi::LogLevel::from_verbosity(debug_level));

        // Read configuration file.
        auto config = edi::get_configuration(config_filename);

        // Prepare flat channel names and SharedPVs
        auto flat_channel_name = config.create_flat_channel_name_vector();
        std::vector<::pvxs::server::SharedPV> pvs(flat_channel_name.size());

        // Initialize socket subsystem.
        edi::SocketContext socketContext;

        // (Optional) configuring logging using $PVA_LOG
        ::pvxs::logger_config_env();

        // Configure a server using $EPICS_PVAS_* or $EPICS_PVA_*
        auto server = ::pvxs::server::Server::fromEnv();

        // Start PVA server.
        server.start();

        // Run receiver.
        edi::pva::Receiver(config, port, listening_address).run(runtime,
            [&flat_channel_name, &pvs, &server](uint32_t channel_id, const ::pvxs::Value& value) {
                const auto& name = flat_channel_name[channel_id];
                auto& pv = pvs[channel_id]; 
                if (!pv) {
                    // instantiate if value is not null (aka disconnected)
                    if (value) {
                        std::cout << "instantiating: " << name << ": " << value;
                        pv = ::pvxs::server::SharedPV::buildReadonly();
                        pv.open(value);
                        server.addPV("x"+name, pv); // TODO added prefix for easy testing
                    }
                } else {
                    if (value) {
                        if (pv.isOpen()) {
                            std::cout << "update: " << name << ": " << value;
                            pv.post(value);    
                        } else {
                            std::cout << "reopen: " << name << ": " << value;
                            pv.open(value);
                        }
                    } else {
                        std::cout << "closing: " << name << std::endl;
                        pv.close();
                    }
                }
            });

        server.stop();

        return 0;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
