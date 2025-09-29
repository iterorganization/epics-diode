#include <string>
#include <vector>
#include <iostream>

#include "testMain.h"
#include "epicsUnitTest.h"

#include <epics-diode/config.h>


namespace edi = epics_diode;

const char* const TEST_EPICS_DIODE_CONFIG_FILENAME("../test_diode_config.json");

const std::size_t REF_HASH = 14358125269606085529ULL;
const double REF_MIN_UPDATE_PERIOD = 0.025;
const double REF_POLLED_FIELDS_UPDATE_PERIOD = 6.0;
const double REF_HEARTBEAT_PERIOD = 30.0;
const uint32_t REF_RATE_LIMIT = 32;
const std::size_t REF_NUMBER_OF_CHANNELS = 8;


void test_channel(
        const edi::ConfigChannel &channel,
        const char *name,
        const int number,
        const std::vector<std::string> &extra_fields,
        const std::vector<std::string> &polled_fields)
{
    if (channel.channel_name.compare(name) == 0) {
        testPass("Channel %d name OK!", number);
    } else {
        testFail("Channel %d name WRONG!", number);
    }

    if (channel.extra_fields.size() == extra_fields.size()) {
        testPass("Channel %d number of extra fields OK!", number);
        
        if (channel.extra_fields.size() > 0) {
            for (unsigned i = 0; i < extra_fields.size(); i++) {
                if (channel.extra_fields[i].compare(extra_fields[i]) == 0) {
                    testPass("Channel %d extra field %d name OK!", number, i);
                } else {
                    testFail("Channel %d extra field %d name WRONG!", number, i);
                }
            }
        }
    } else {
        testFail("Channel %d number of extra fields WRONG!", number);
    }

    if (channel.polled_fields.size() == polled_fields.size()) {
        testPass("Channel %d number of polled fields OK!", number);

        if (channel.polled_fields.size() > 0) {
            for (unsigned i = 0; i < polled_fields.size(); i++) {
                if (channel.polled_fields[i].compare(polled_fields[i]) == 0) {
                    testPass("Channel %d polled field %d name OK!", number, i);
                } else {
                    testFail("Channel %d polled field %d name WRONG!", number, i);
                }
            }
        }
    } else {
        testFail("Channel %d number of polled fields WRONG!", number);
    }

}

MAIN(test_diode) {
    testPlan(0);

    try {
        std::string config_filename = TEST_EPICS_DIODE_CONFIG_FILENAME;

        // Read configuration file.
        auto config = edi::get_configuration(config_filename);

        //std::cout << config << std::endl;
        //std::cout << "hash: " << config.hash << std::endl;

        // Compare with expected reference values
        if (config.hash == REF_HASH) {
            testPass("Hash OK!");
        } else {
            testFail("Hash FAILED (%zu != %zu)!", REF_HASH, config.hash);
        }

        if (config.min_update_period == REF_MIN_UPDATE_PERIOD) {
            testPass("Min update period OK!");
        } else {
            testFail("Min update period FAILED!");
        }

        if (config.polled_fields_update_period == REF_POLLED_FIELDS_UPDATE_PERIOD) {
            testPass("Polled fields update period OK!");
        } else {
            testFail("Polled fields update period FAILED!");
        }

        if (config.heartbeat_period == REF_HEARTBEAT_PERIOD) {
            testPass("Heartbeat period OK!");
        } else {
            testFail("Heartbeat period FAILED!");
        }

        if (config.rate_limit_mbs == REF_RATE_LIMIT) {
            testPass("Rate limit OK!");
        } else {
            testFail("Rate limit FAILED!");
        }

        if (config.channels.size() == REF_NUMBER_OF_CHANNELS) {
            testPass("Channels size OK!");
        } else {
            testFail("Channels size WRONG!");
        }

        auto &channel1 = config.channels[0];
        auto &channel2 = config.channels[1];
        auto &channel3 = config.channels[2];
        auto &channel4 = config.channels[3];
        auto &channel5 = config.channels[4];
        auto &channel6 = config.channels[5];
        auto &channel7 = config.channels[6];
        auto &channel8 = config.channels[7];

        test_channel(channel1, "poz:ai1", 1, { "RVAL" }, { "SVAL" } );
        test_channel(channel2, "poz:ai2", 2, { "RVAL" }, {});
        test_channel(channel3, "poz:ai3", 3, { "RVAL" }, {});
        test_channel(channel4, "poz:compressExample", 4, {}, {});
        test_channel(channel5, "poz:image", 5, {}, {});
        test_channel(channel6, "poz:one_element", 6, {}, {});
        test_channel(channel7, "poz:stalled", 7, {}, { "RVAL" });
        test_channel(channel8, "poz:enum", 8, {}, {});

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        testFail("FAIL: Exception!");
    }

    return testDone();
}
