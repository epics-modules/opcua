cd "$(OPCUA_TEST_TOP)"

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
# Session debug output (the pytest fixture sets OPCUA_DEBUG=1)
opcuaOptions("$(SESSION)", "debug=$(OPCUA_DEBUG=0)")

# Test database
dbLoadRecords("end2endTest/ioc/test.db", "OPCSUB=$(SUBSCRIPT),NS=$(OPCNAMESPACE)")
# 64-bit array types require EPICS Base >= 3.16.1 (set NO_INT64="#" to skip)
$(NO_INT64=)dbLoadRecords("end2endTest/ioc/test_int64.db", "OPCSUB=$(SUBSCRIPT),NS=$(OPCNAMESPACE)")

iocInit()
