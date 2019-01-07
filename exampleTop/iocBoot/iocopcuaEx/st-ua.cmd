#!../../bin/linux-x86_64/opcuaIoc

< envPaths

dbLoadDatabase("../../dbd/opcuaIoc.dbd",0,0)
opcuaIoc_registerRecordDeviceDriver(pdbbase) 

epicsEnvSet("SESSION",   "OPC1")
epicsEnvSet("SUBSCRIPT", "SUB1")
epicsEnvSet("OPCSERVER", "127.0.0.1")

iocshLoad("$(TOP)/templates/iocsh/opcua.iocsh", "P=OPC:,SESS=$(SESSION),SUBS=$(SUBSCRIPT),INET=$(OPCSERVER)")

dbLoadRecords("$(TOP)/db/UaAnsiCServer.db","P=OPC:,R=Demo:,SESS=$(SESSION),SUBS=$(SUBSCRIPT)")

iocInit()

