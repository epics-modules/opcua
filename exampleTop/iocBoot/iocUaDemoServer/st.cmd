#!../../bin/linux-x86_64/opcuaIoc

## You may have to change opcuaIoc to something else
## everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/opcuaIoc.dbd"
opcuaIoc_registerRecordDeviceDriver pdbbase

# Pretty minimal setup: one session with a 200ms subscription on top
opcuaSession OPC1 opc.tcp://localhost:48010
opcuaSubscription SUB1 OPC1 200

# Switch off security
opcuaOptions OPC1 sec-mode=None

# Set up a namespace mapping
# (the databases use ns=2, but the demo server >=v1.8 uses ns=3)

opcuaMapNamespace OPC1 2 "http://www.unifiedautomation.com/DemoServer/"

# Load the databases for the UaServerCpp demo server
# (you can set DEBUG=<n>) to set default values in all TPRO fields)

dbLoadRecords "db/UaDemoServer-server.db", "P=OPC:,R=,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Dynamic.Arrays.db", "P=OPC:,R=DDA:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Dynamic.Scalar.db", "P=OPC:,R=DDS:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Static.Arrays.db", "P=OPC:,R=DSA:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Static.Scalar.db", "P=OPC:,R=DSS:,SESS=OPC1,SUBS=SUB1"

dbLoadRecords "db/Demo.WorkOrder.db", "P=OPC:,SESS=OPC1,SUBS=SUB1"

# DO NOT LOAD THESE DBs ON EPICS BASE < 7.0     \/  \/  \/     EPICS 7 ONLY
# int64 and long string records need EPICS 7
dbLoadRecords "db/Demo.Dynamic.ScalarE7.db", "P=OPC:,R=DDS:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Dynamic.ArraysE7.db", "P=OPC:,R=DDA:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Static.ScalarE7.db", "P=OPC:,R=DSS:,SESS=OPC1,SUBS=SUB1"
dbLoadRecords "db/Demo.Static.ArraysE7.db", "P=OPC:,R=DSA:,SESS=OPC1,SUBS=SUB1"

iocInit
