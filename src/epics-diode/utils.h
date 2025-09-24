/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_UTILS_H
#define EPICS_DIODE_UTILS_H

#include <cstdio>

#ifndef _WIN32
#  define LINE_BUFFER(stream) setvbuf(stream, NULL, _IOLBF, BUFSIZ)
#else
/* Windows doesn't support line mode, turn buffering off completely */
#  define LINE_BUFFER(stream) setvbuf(stream, NULL, _IONBF, 0)
#endif

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)


namespace epics_diode {

uint64_t value_hash(const void* value, const uint32_t size);

}



#endif
