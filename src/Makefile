TOP = ..

include $(TOP)/configure/CONFIG

EXPANDVARS += EPICS_DIODE_MAJOR_VERSION
EXPANDVARS += EPICS_DIODE_MINOR_VERSION
EXPANDVARS += EPICS_DIODE_MAINTENANCE_VERSION
EXPANDVARS += EPICS_DIODE_DEVELOPMENT_FLAG

EXPANDFLAGS += $(foreach var,$(EXPANDVARS),-D$(var)="$(strip $($(var)))")

INC += epics-diode/version.h
INC += epics-diode/versionNum.h
INC += epics-diode/protocol.h
INC += epics-diode/transport.h
INC += epics-diode/config.h
INC += epics-diode/logger.h
INC += epics-diode/sender.h
INC += epics-diode/receiver.h
INC += epics-diode/utils.h

LIBRARY += epics-diode
epics-diode_SRCS += protocol.cpp
epics-diode_SRCS += transport.cpp
epics-diode_SRCS += config.cpp
epics-diode_SRCS += logger.cpp
epics-diode_SRCS += sender.cpp
epics-diode_SRCS += receiver.cpp
epics-diode_SRCS += utils.cpp

epics-diode_LIBS += Com ca




INC += epics-diode/pva/sender.h
INC += epics-diode/pva/receiver.h

LIBRARY += epics-pva-diode
epics-pva-diode_SRCS += pva_sender.cpp
epics-pva-diode_SRCS += pva_receiver.cpp
epics-pva-diode_SRCS += pva_protocol.cpp

# import pvxs source code, PVXS must be defined in RELEASE[.local]
#USR_INCLUDES += -I$(PVXS)/src
USR_INCLUDES += -I$(TOP)/pvxs_headers_1_1_1
epics-pva-diode_LIBS += Com epics-diode pvxs



PROD += diode_sender
diode_sender_SRCS += diode_sender.cpp
diode_sender_LIBS = Com ca epics-diode

PROD += diode_receiver
diode_receiver_SRCS += diode_receiver.cpp
diode_receiver_LIBS = Com ca epics-diode

PROD += diode_dbgen
diode_dbgen_SRCS += diode_dbgen.cpp
diode_dbgen_LIBS = Com ca epics-diode

PROD += pvadiode_sender
pvadiode_sender_SRCS += pvadiode_sender.cpp
pvadiode_sender_LIBS = Com epics-diode epics-pva-diode pvxs

PROD += pvadiode_receiver
pvadiode_receiver_SRCS += pvadiode_receiver.cpp
pvadiode_receiver_LIBS = Com epics-diode epics-pva-diode pvxs

SHRLIB_VERSION ?= $(EPICS_DIODE_MAJOR_VERSION).$(EPICS_DIODE_MINOR_VERSION).$(EPICS_DIODE_MAINTENANCE_VERSION)

include $(TOP)/configure/RULES

# Can't use EXPAND as generated headers must appear
# in O.Common, but EXPAND emits rules for O.$(T_A)
../O.Common/epics-diode/versionNum.h: ../epics-diode/versionNum.h@
	$(MKDIR) $(COMMON_DIR)/epics-diode
	$(EXPAND_TOOL) $(EXPANDFLAGS) $($@_EXPANDFLAGS) $< $@
