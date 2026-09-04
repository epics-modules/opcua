# Load and register IOC support
dbLoadDatabase("dbd/opcuaTestIoc.dbd")
opcuaTestIoc_registerRecordDeviceDriver(pdbbase)

# Test server
epicsEnvSet("OPCSERVER", "127.0.0.1")
epicsEnvSet("OPCPORT", "4840")
epicsEnvSet("OPCNAMESPACE", "2")

# OPC UA configuration
epicsEnvSet("SESSION", "OPC1")
epicsEnvSet("SUBSCRIPT", "SUB1")

opcuaSession("$(SESSION)", "opc.tcp://$(OPCSERVER):$(OPCPORT)")
opcuaSubscription("$(SUBSCRIPT)", "$(SESSION)", 100)
opcuaOptions("$(SESSION)", "sec-mode=None")

# Test database
dbLoadRecords("end2endTest/ioc/test.db", "OPCSUB=$(SUBSCRIPT),NS=$(OPCNAMESPACE)")

iocInit()
