#!../../bin/linux-x86_64/source

#- You may have to change source to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/source.dbd"
source_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadTemplate("db/poz.substitutions", "POZ=poz")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=root"
