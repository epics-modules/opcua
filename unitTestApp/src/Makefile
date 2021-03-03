TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DEVSUP_SRC = $(TOP)/devOpcuaSup

USR_INCLUDES += -I$(DEVSUP_SRC)
SRC_DIRS += $(DEVSUP_SRC)

TESTSRC = $(TOP)/unitTestApp/src

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
LinkParserTest_LIBS_DEFAULT += opcua
LinkParserTest_OBJS_WIN32 += linkParser iocshIntegration
LinkParserTest_OBJS_WIN32 += SessionUaSdk SubscriptionUaSdk ItemUaSdk DataElementUaSdk
LinkParserTest_OBJS_WIN32 += RecordConnector Session Subscription
LinkParserTest_LIBS_WIN32 += $(EPICS_BASE_IOC_LIBS)
GTESTS += LinkParserTest

GTESTPROD_HOST += ElementTreeTest
ElementTreeTest_SRCS += ElementTreeTest.cpp
ElementTreeTest_LIBS_DEFAULT += opcua
ElementTreeTest_OBJS_WIN32 += linkParser iocshIntegration
ElementTreeTest_OBJS_WIN32 += SessionUaSdk SubscriptionUaSdk ItemUaSdk DataElementUaSdk
ElementTreeTest_OBJS_WIN32 += RecordConnector Session Subscription
ElementTreeTest_LIBS_WIN32 += $(EPICS_BASE_IOC_LIBS)
GTESTS += ElementTreeTest

#==================================================
# Tests for different client libraries
# are in separate directories, added by reading
# Makefile fragments

# Unified Automation C++ Based OPC UA Client SDK
ifdef UASDK
include $(TESTSRC)/UaSdk/Makefile
endif

USR_LIBS += Com

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
