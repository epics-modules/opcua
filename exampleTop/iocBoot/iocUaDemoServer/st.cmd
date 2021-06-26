#!../../bin/linux-x86_64/opcuaIoc

## You may have to change opcuaIoc to something else
## everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/opcuaIoc.dbd"
opcuaIoc_registerRecordDeviceDriver pdbbase

# Pretty minimal setup: one session with a 200ms subscription on top
opcuaCreateSession OPC1 opc.tcp://localhost:48010
opcuaCreateSubscription SUB1 OPC1 200

# Switch off security
opcuaSetOption OPC1 sec-mode None

# Load the databases for the UaServerCpp demo server

dbLoadRecords "db/UaDemoServer-server.db", "P=OPC:,R=,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Dynamic.Arrays.db", "P=OPC:,R=DDA:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Dynamic.Scalar.db", "P=OPC:,R=DDS:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Static.Arrays.db", "P=OPC:,R=DSA:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Static.Scalar.db", "P=OPC:,R=DSS:,SESS=OPC1,SUBS=SUB1"

dbLoadRecords "db/Demo.WorkOrder.db", "P=OPC:,SESS=OPC1,SUBS=SUB1"

# int64 and long string records need EPICS 7
dbLoadRecords "db/Demo.Dynamic.ScalarE7.db", "P=OPC:,R=DDS:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Dynamic.ArraysE7.db", "P=OPC:,R=DDA:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Static.ScalarE7.db", "P=OPC:,R=DSS:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Static.ArraysE7.db", "P=OPC:,R=DSA:,SESS=OPC1,SUBS=SUB1"

iocInit
