TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_INCLUDES += -I$(TOP)/devOpcuaSup

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
