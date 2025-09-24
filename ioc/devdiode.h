/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_DEVDIODE_H
#define EPICS_DIODE_DEVDIODE_H

#include <stdint.h>
#include <epicsTypes.h>
#include <epicsTime.h>

#ifdef __cplusplus
extern "C" {
#endif

struct meta_data {
  epicsInt16 status;
  epicsInt16 severity;
  epicsTimeStamp stamp; 
};

int diode_assign(uint32_t channel_index, const char* name);
int diode_assign_fields();

int diode_value_update(uint32_t channel_index, uint16_t type, uint32_t count,
                       struct meta_data* meta, void* value, uint64_t hash);

int diode_disconnected(uint32_t channel_index);

#ifdef __cplusplus
}
#endif 

#endif 
