TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = source
# source.dbd will be created and installed
DBD += source.dbd

# source.dbd will be made up from these files:
source_DBD += base.dbd

# Include dbd files from all support applications:
#source_DBD += xxx.dbd

# Add all the support libraries needed by this IOC
#source_LIBS += xxx

# source_registerRecordDeviceDriver.cpp derives from source.dbd
source_SRCS += source_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
source_SRCS_DEFAULT += sourceMain.cpp
source_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#source_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
source_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

