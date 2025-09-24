/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_VERSION_H
#define EPICS_DIODE_VERSION_H

#include <epics-diode/versionNum.h>

#ifndef VERSION_INT
#  define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
#endif

#define EPICS_DIODE_VERSION_INT VERSION_INT(EPICS_DIODE_MAJOR_VERSION, EPICS_DIODE_MINOR_VERSION, EPICS_DIODE_MAINTENANCE_VERSION, 0)

#endif
