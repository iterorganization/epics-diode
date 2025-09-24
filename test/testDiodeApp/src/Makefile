TOP = ../../..

include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DBD += testDiode.dbd

PROD_IOC += testDiode

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
testDiode_SRCS_DEFAULT += testDiode_registerRecordDeviceDriver.cpp testDiodeMain.cpp
testDiode_LIBS += diodeIoc
testDiode_LIBS += epics-diode
testDiode_LIBS += $(EPICS_BASE_IOC_LIBS)

CFG += test_diode.json readOnly.acf

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
