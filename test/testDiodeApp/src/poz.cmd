cd ../../..

dbLoadDatabase("dbd/testDiode.dbd")
testDiode_registerRecordDeviceDriver(pdbbase)

# Trace = 0, Debug = 1, Config = 2, Info = 3 (default), Warning = 4, Error = 5 
diodeLogLevel(3)

# POZ side (usually started as standalone diode_sender process and not deployed within the same IOC/network as XPOZ)
# NOTE: do not start after IOC CA initialzation (i.e. after IOC installs local CA service)
diodeSenderStart("cfg/test_diode.json", "localhost:5080")

# POZ side (usually not deployed within the same IOC/network as XPOZ)
dbLoadTemplate("db/poz.substitutions")
dbLoadRecords("db/testDiode.db", "prefix=poz")

iocInit()
