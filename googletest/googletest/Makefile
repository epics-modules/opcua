TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#================================================
# Build GoogleTest libraries gtest and gtest_main

ifdef GTEST_HOME

GTEST = $(TOP)/googletest/googletest

LIBRARY_HOST += gtest
LIBRARY_HOST += gtest_main

SRC_DIRS += $(GTEST)/src
USR_INCLUDES += -I$(GTEST)/include

gtest_SRCS += gtest-all.cc
gtest_main_SRCS += gtest_main.cc

SHARED_LIBRARIES = NO

endif
#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
