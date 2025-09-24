asSetFilename("/epics-diode/cfg/readOnly.acf")

dbLoadDatabase("/epics-diode/dbd/testDiode.dbd")
testDiode_registerRecordDeviceDriver(pdbbase)

# Trace = 0, Debug = 1, Config = 2, Info = 3 (default), Warning = 4, Error = 5 
diodeLogLevel(3)

cd 
# XPOZ side
dbLoadRecords("./test.db")

diodeReceiverStart("/test_config/diode.json", 5080, "0.0.0.0")

diodeIocInit()
