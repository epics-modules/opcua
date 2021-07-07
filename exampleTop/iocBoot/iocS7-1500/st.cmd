#!../../bin/linux-x86_64/opcuaIoc

## You may have to change opcuaIoc to something else
## everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/opcuaIoc.dbd"
opcuaIoc_registerRecordDeviceDriver pdbbase

## Pretty minimal setup: one session with a 200ms subscription on top
opcuaSession OPC1 opc.tcp://localhost:4840
opcuaSubscription SUB1 OPC1 200

# Switch off security
opcuaOptions OPC1 sec-mode=None

## Load the databases for one of the examples

## Siemens S7-1500 PLC
dbLoadRecords "db/S7-1500-server.db", "P=OPC:,R=,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/S7-1500-DB1.db", "P=OPC:,R=DB1:,SESS=OPC1,SUBS=SUB1"

iocInit

## Start any sequence programs
#seq sncopcuaIoc,"user=ralph"
