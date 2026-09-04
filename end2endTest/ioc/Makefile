TOP = ../..

include $(TOP)/configure/CONFIG

PROD_IOC += opcuaTestIoc

DBD += opcuaTestIoc.dbd

opcuaTestIoc_DBD += base.dbd

# PVA/QSRV
opcuaTestIoc_DBD += PVAServerRegister.dbd
opcuaTestIoc_DBD += qsrv.dbd

# OPC UA
opcuaTestIoc_DBD += menuDefAction.dbd
opcuaTestIoc_DBD += opcuaItemRecord.dbd
opcuaTestIoc_DBD += devOpcua.dbd

opcuaTestIoc_SRCS += opcuaTestIoc_registerRecordDeviceDriver.cpp
opcuaTestIoc_SRCS_DEFAULT += opcuaTestIocMain.cpp

opcuaTestIoc_LIBS += opcua

# PVA/QSRV
opcuaTestIoc_LIBS += qsrv
opcuaTestIoc_LIBS += $(EPICS_BASE_PVA_CORE_LIBS)

opcuaTestIoc_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
