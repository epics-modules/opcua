TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

ifdef GTEST_HOME

#==================================================
# Build EPICS gtest_main library (uses TAP output)

LIBRARY_HOST += epics_gtest_main
epics_gtest_main_SRCS += gtest_main.cc
epics_gtest_main_LIBS += gtest

ifneq ($(GTEST_HOME),SYSTEM)
USR_INCLUDES += -I$(GTEST_HOME)/googletest/include
gtest_DIR = $(GTEST_HOME)/googletest
endif

CFG += RULES_GTEST
CFG += CONFIG_GTEST

endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
