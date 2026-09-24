TOP = ../..

include $(TOP)/configure/CONFIG

ifdef T_A
include $(TOP)/cfg/CONFIG_OPCUA
endif

ifdef PVA2PVA
  USR_DBDFLAGS += -I$(PVA2PVA)/dbd
  USR_LDFLAGS += -L$(PVA2PVA)/lib/$(T_A)
endif

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

ifdef UASDK
  ifeq ($(UASDK_DEPLOY_MODE),PROVIDED)
    USR_LDFLAGS_Linux += -Wl,--disable-new-dtags
    USR_LDFLAGS_Linux += -Wl,-rpath,$(UASDK_LIB_DIR)
  endif
endif

include $(TOP)/configure/RULES
