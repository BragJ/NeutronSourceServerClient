#!../../bin/linux-x86_64/neutrons

< envPaths

cd ${TOP}

## Register all support components
dbLoadDatabase("dbd/neutrons.dbd")
neutrons_registerRecordDeviceDriver(pdbbase)

## Load record instances
dbLoadRecords("db/neutrons.db","P=demo")

cd ${TOP}/iocBoot/${IOC}
iocInit()

# List V3 records
dbl
epicsThreadSleep(1.0)

# When not loading records to control the V4 "neutrons" server,
# this call can be used as an alternative.
# The events created that way can not be configured at runtime
# via V3 records!
#neutronServerCreateRecord("neutrons", 0.1, 10)
#neutronServerCreateRecord("neutrons0", 0.01, 10)	

startPVAServer

# List V4 channels
pvdbl

#
# If IOC includes pvaSrv,
# the V3 records are also served via pvAccess.
# This is a build time option, see srcIoc/src/neutronsInclude.dbd

