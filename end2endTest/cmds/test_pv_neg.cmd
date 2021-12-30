# OPC simulation server
epicsEnvSet("OPCSERVER", "127.0.0.1")
epicsEnvSet("OPCPORT", "4840")
epicsEnvSet("OPCNAMESPACE", "2")

# OPCUA environment variables
epicsEnvSet("SESSION",   "OPC1")
epicsEnvSet("SUBSCRIPT", "SUB1")

# Load OPCUA module startup script
iocshLoad("$(opcua_DIR)/opcua.iocsh", "P=OPC:,SESS=$(SESSION),SUBS=$(SUBSCRIPT),INET=$(OPCSERVER),PORT=$(OPCPORT)")

dbLoadRecords("test_pv_neg.db", "OPCSUB=$(SUBSCRIPT), NS=$(OPCNAMESPACE)")

iocInit()

