#*************************************************************************
# Copyright (c) 2020 ITER Organization.
# This module is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Author: Ralph Lange <ralph.lange@gmx.de>

# This is a Makefile fragment, see unitTestApp/src/Makefile.

SRC_DIRS += $(TESTSRC)/UaSdk
USR_INCLUDES += -I$(TOP)/devOpcuaSup/UaSdk

#==================================================
# Build tests executables

GTESTPROD_HOST += RangeCheckTest
RangeCheckTest_SRCS += RangeCheckTest.cpp
GTESTS += RangeCheckTest
