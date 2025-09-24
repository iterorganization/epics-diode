#!../../bin/linux-x86_64/source

#- You may have to change source to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/source.dbd"
source_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords("db/test.db","POZ=poz,N=1")
dbLoadRecords("db/test.db","POZ=poz,N=2")
dbLoadRecords("db/test.db","POZ=poz,N=3")
dbLoadRecords("db/test.db","POZ=poz,N=4")
dbLoadRecords("db/test.db","POZ=poz,N=5")
dbLoadRecords("db/test.db","POZ=poz,N=6")
dbLoadRecords("db/test.db","POZ=poz,N=7")
dbLoadRecords("db/test.db","POZ=poz,N=8")
dbLoadRecords("db/test.db","POZ=poz,N=9")
dbLoadRecords("db/test.db","POZ=poz,N=10")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=root"
