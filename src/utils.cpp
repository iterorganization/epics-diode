/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <string.h>

#include <epicsString.h>

#include <epics-diode/logger.h>
#include <epics-diode/utils.h>

namespace epics_diode {


uint64_t value_hash(const void* value, const uint32_t size) {
    if (size <= sizeof(uint64_t)) {
        uint64_t hash = 0;
        memcpy(&hash, value, size);
        return hash;
    } else {
        return uint64_t(epicsMemHash((char*) value, size, 0));
    }
}


}
