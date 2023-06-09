TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DEVSUP_SRC = $(TOP)/devOpcuaSup

USR_INCLUDES += -I$(DEVSUP_SRC)
SRC_DIRS += $(DEVSUP_SRC)

TESTSRC = $(TOP)/unitTestApp/src

# Link against objects as libopcua might be static (deploy mode EMBED)
OPCUA_OBJS += linkParser iocshIntegration
ifdef UASDK
OPCUA_OBJS += SessionUaSdk SubscriptionUaSdk ItemUaSdk DataElementUaSdk
endif
OPCUA_OBJS += RecordConnector Session Subscription

#==================================================
# Build tests executables

GTESTPROD_HOST += UpdateTest
UpdateTest_SRCS += UpdateTest.cpp
GTESTS += UpdateTest

GTESTPROD_HOST += UpdateQueueTest
UpdateQueueTest_SRCS += UpdateQueueTest.cpp
GTESTS += UpdateQueueTest

GTESTPROD_HOST += RequestQueueBatcherTest
RequestQueueBatcherTest_SRCS += RequestQueueBatcherTest.cpp
GTESTS += RequestQueueBatcherTest

GTESTPROD_HOST += RegistryTest
RegistryTest_SRCS += RegistryTest.cpp
GTESTS += RegistryTest

GTESTPROD_HOST += LinkParserTest
LinkParserTest_SRCS += LinkParserTest.cpp
LinkParserTest_LIBS += $(OPCUA_LIBS) $(EPICS_BASE_IOC_LIBS)
LinkParserTest_SYS_LIBS_Linux += $(OPCUA_SYS_LIBS_Linux)
LinkParserTest_OBJS += $(OPCUA_OBJS)
GTESTS += LinkParserTest

GTESTPROD_HOST += ElementTreeTest
ElementTreeTest_SRCS += ElementTreeTest.cpp
ElementTreeTest_LIBS += $(OPCUA_LIBS) $(EPICS_BASE_IOC_LIBS)
ElementTreeTest_SYS_LIBS_Linux += $(OPCUA_SYS_LIBS_Linux)
ElementTreeTest_OBJS += $(OPCUA_OBJS)
GTESTS += ElementTreeTest

#==================================================
# Tests for different client libraries
# are in separate directories, added by reading
# Makefile fragments

# Unified Automation C++ Based OPC UA Client SDK
ifdef UASDK
include $(TESTSRC)/UaSdk/Makefile.config
endif

USR_LIBS += Com

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# Unified Automation C++ Based OPC UA Client SDK
ifdef UASDK
include $(DEVSUP_SRC)/UaSdk/Makefile.rules
endif
