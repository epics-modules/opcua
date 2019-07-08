TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_INCLUDES += -I$(TOP)/devOpcuaSup

#==================================================
# Build tests executables

GTESTPROD_HOST += UpdateTest
UpdateTest_SRCS += UpdateTest.cpp
TESTS += UpdateTest

GTESTPROD_HOST += UpdateQueueTest
UpdateQueueTest_SRCS += UpdateQueueTest.cpp
TESTS += UpdateQueueTest

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

USR_LIBS += Com

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
