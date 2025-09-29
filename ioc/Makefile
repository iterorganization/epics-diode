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

USR_CPPFLAGS += -DUSE_TYPED_RSET -DUSE_TYPED_DSET

DBD += diode.dbd

LIBRARY_IOC += diodeIoc
diodeIoc_SRCS += diodeIoc.cpp
diodeIoc_SRCS += devdiode.c
diodeIoc_SRCS += iocInit.c

diodeIoc_LIBS += epics-diode
diodeIoc_LIBS += $(EPICS_BASE_IOC_LIBS)

SHRLIB_VERSION ?= $(EPICS_DIODE_MAJOR_VERSION).$(EPICS_DIODE_MINOR_VERSION).$(EPICS_DIODE_MAINTENANCE_VERSION)

#===========================

include $(TOP)/configure/RULES

# Can't use EXPAND as generated headers must appear
# in O.Common, but EXPAND emits rules for O.$(T_A)
../O.Common/epics-diode/versionNum.h: ../epics-diode/versionNum.h@
	$(MKDIR) $(COMMON_DIR)/epics-diode
	$(EXPAND_TOOL) $(EXPANDFLAGS) $($@_EXPANDFLAGS) $< $@
