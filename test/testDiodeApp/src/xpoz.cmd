cd ../../..

asSetFilename("cfg/readOnly.acf")

dbLoadDatabase("dbd/testDiode.dbd")
testDiode_registerRecordDeviceDriver(pdbbase)

# Trace = 0, Debug = 1, Config = 2, Info = 3 (default), Warning = 4, Error = 5 
diodeLogLevel(3)

# XPOZ side
dbLoadRecords("db/testDiodeX.db", "prefix=xpoz")

diodeReceiverStart("cfg/test_diode.json", 5080, "0.0.0.0")

diodeIocInit()
