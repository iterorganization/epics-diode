#!"$(CODAC_ROOT)/apps/epics-diode/bin/testDiode"

## Set paths
epicsEnvSet("DIODE_ROOT", "$(CODAC_ROOT)/apps/epics-diode")

asSetFilename("$(DIODE_ROOT)/cfg/readOnly.acf")

dbLoadDatabase("$(DIODE_ROOT)/dbd/testDiode.dbd")
testDiode_registerRecordDeviceDriver(pdbbase)

# Trace = 0, Debug = 1, Config = 2, Info = 3 (default), Warning = 4, Error = 5
diodeLogLevel(3)

# XPOZ side
# dbLoadRecords("$(DIODE_ROOT)/db/testDiodeX.db")

diodeReceiverStart("$(DIODE_ROOT)/cfg/diode.json", 5080, "0.0.0.0")

diodeIocInit()

