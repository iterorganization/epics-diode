/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <cstdio>
#include <atomic>
#include <iostream>

#include <epicsExport.h>
#include <epicsTypes.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <errlog.h>
#include <iocsh.h>
#include <initHooks.h>
#include <epicsExit.h>

#include <cadef.h>
#include <menuFtype.h>

#include "devdiode.h"
#include "iocshelper.h"

#include <epics-diode/config.h>
#include <epics-diode/logger.h>
#include <epics-diode/receiver.h>
#include <epics-diode/sender.h>
#include <epics-diode/transport.h>
#include <epics-diode/version.h>
#include <epics-diode/utils.h>

#ifdef __cplusplus
extern "C" {
#endif
void diode_IocRegister(void);
#ifdef __cplusplus
}
#endif 


namespace edi = epics_diode;

namespace {

// see dbFldTypes.h
uint16_t dbr_to_fvtype[] = {
    menuFtypeSTRING,
    menuFtypeSHORT,
    menuFtypeFLOAT,
    menuFtypeENUM,
    menuFtypeCHAR,
    menuFtypeLONG,
    menuFtypeDOUBLE
};

std::atomic<bool> shutdown_flag;

void diodeLogLevel(int log_level)
{
    edi::Logger::set_default_log_level(edi::LogLevel::Level(log_level));
}


struct ReceiverParams {
    ReceiverParams(const std::string &config_filename, int socket_port, const char* listening_address) :
        config_filename(config_filename),
        socket_port(socket_port),
        listening_address(edi::EPICS_DIODE_DEFAULT_LISTENING_ADDRESS)
    {
        if (listening_address != NULL) {
            this->listening_address = listening_address;
        }
    }

    std::string config_filename;
    int socket_port = 0;
    std::string listening_address;
};

static void receiverTask(void *pvt)
{
    try {
        std::unique_ptr<ReceiverParams> params(static_cast<ReceiverParams*>(pvt));

        edi::Logger logger("receiverTask");
        logger.log(edi::LogLevel::Debug, "epics-diode receiver task started.");

        // Read configuration file.
        auto config = edi::get_configuration(params->config_filename);

        // Assign channel indexes.
        uint32_t channel_index = 0;
        for (auto &config_channel : config.channels) {
            diode_assign(channel_index++, config_channel.channel_name.c_str());

            for (auto &field_name : config_channel.extra_fields) {
                diode_assign(channel_index++, (config_channel.channel_name + "." + field_name).c_str());
            } 
            for (auto &field_name : config_channel.polled_fields) {
                diode_assign(channel_index++, (config_channel.channel_name + "." + field_name).c_str());
            } 
        }

        // Initialize socket subsystem.
        edi::SocketContext socketContext;

        // Run receiver.
        edi::Receiver receiver(config, params->socket_port, params->listening_address);
        auto callback = 
            [](uint32_t channel_id, uint16_t type, uint32_t count, void* value) {
                if (count == (uint32_t)-1) {
                    diode_disconnected(channel_id);
                } else {
                    // we expect dbr_time_* or dbr_* type

                    if (dbr_type_is_TIME(type)) {
                        uint16_t base_type = type - DBR_TIME_STRING;
                        auto value_ptr = dbr_value_ptr(value, type);
                        diode_value_update(channel_id,
                                dbr_to_fvtype[base_type],
                                count,
                                (struct meta_data*)value,
                                value_ptr,
                                edi::value_hash(value_ptr, dbr_value_size[base_type]*count));
                    } else if (dbr_type_is_plain(type)) {
                        diode_value_update(channel_id,
                                dbr_to_fvtype[type],
                                count,
                                NULL,
                                value,
                                edi::value_hash(value, dbr_value_size[type]*count));
                    } else {
                        errlogPrintf("epics-diode: !dbr_type_is_TIME(type) && !dbr_type_is_plain(type)\n");
                    }
                }
            };

        while (!shutdown_flag.load()) {
            receiver.run(1.0, callback);
        }

        logger.log(edi::LogLevel::Debug, "epics-diode receiver task stopped.");

    } catch (std::exception &ex) {
        errlogPrintf("epics-diode:receiverTask: exception caught: %s\n", ex.what());
    }
}


void diodeReceiverStart(const char* config_filename, int socket_port, const char* listening_address)
{
    //auto params = std::make_unique<ReceiverParams>(config_filename, socket_port);
    auto params = std::unique_ptr<ReceiverParams>(new ReceiverParams(config_filename, socket_port, listening_address));
    epicsThreadId thread_id = epicsThreadMustCreate("diode receiver",
                                    epicsThreadPriorityMedium,
                                    epicsThreadGetStackSize(epicsThreadStackMedium),
                                    (EPICSTHREADFUNC)::receiverTask,
                                    params.get());
    if (thread_id) {
        params.release();
    }
}





struct SenderParams {
    SenderParams(const std::string &config_filename, const std::string &sender_addresses) :
        config_filename(config_filename),
        sender_addresses(sender_addresses) {
    }

    std::string config_filename;
    std::string sender_addresses;
};

static void senderTask(void *pvt)
{
    try {
        std::unique_ptr<SenderParams> params(static_cast<SenderParams*>(pvt));

        edi::Logger logger("senderTask");
        logger.log(edi::LogLevel::Debug, "epics-diode sender task started.");

        // Read configuration file.
        auto config = edi::get_configuration(params->config_filename);

        // Initialize socket subsystem.
        edi::SocketContext socketContext;

        // Run receiver.
        edi::Sender sender(config, params->sender_addresses);
        while (!shutdown_flag.load()) {
            sender.run(1.0);
        }

        logger.log(edi::LogLevel::Debug, "epics-diode sender task stopped.");

    } catch (std::exception &ex) {
        errlogPrintf("epics-diode:senderTask: exception caught: %s\n", ex.what());
    }
}

void diodeSenderStart(const char* config_filename, const char* send_addresses)
{
    //auto params = std::make_unique<SenderParams>(config_filename, send_addresses);
    auto params = std::unique_ptr<SenderParams>(new SenderParams(config_filename, send_addresses));
    epicsThreadId thread_id = epicsThreadMustCreate("diode sender",
                                    epicsThreadPriorityMedium,
                                    epicsThreadGetStackSize(epicsThreadStackMedium),
                                    (EPICSTHREADFUNC)::senderTask,
                                    params.get());
    if (thread_id) {
        params.release();
    }
}


void diodeAtExit(void*)
{
    shutdown_flag.store(true);
}

void diodeInitHook(initHookState state)
{
    if (state == initHookAfterInitDatabase) {
        // we want to run before exitDatabase
        epicsAtExit(&diodeAtExit, nullptr);
    }
}

void diodeRegistrar(void)
{
    epics::iocshRegister<int, &diodeLogLevel>("diodeLogLevel", "log_level");
    epics::iocshRegister<const char*, int, const char*, &diodeReceiverStart>("diodeReceiverStart", "config_filename", "socket_port", "listening_address");
    epics::iocshRegister<const char*, const char*, &diodeSenderStart>("diodeSenderStart", "config_filename", "send_addresses");
    
    diode_IocRegister();

    initHookRegister(&diodeInitHook);
}

}

extern "C" {
  epicsExportRegistrar(diodeRegistrar);
}
