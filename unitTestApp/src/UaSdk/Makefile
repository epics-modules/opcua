#*************************************************************************
# Copyright (c) 2020-2021 ITER Organization.
# This module is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Author: Ralph Lange <ralph.lange@gmx.de>

# This is a Makefile fragment, see unitTestApp/src/Makefile.

DEVSUP_SRC = $(TOP)/devOpcuaSup
UASDK_SRC = $(DEVSUP_SRC)/UaSdk

SRC_DIRS += $(TESTSRC)/UaSdk
SRC_DIRS += $(DEVSUP_SRC) $(UASDK_SRC)
USR_INCLUDES += -I$(UASDK_SRC)

#==================================================
# Build tests executables

GTESTPROD_HOST += RangeCheckTest
RangeCheckTest_SRCS += RangeCheckTest.cpp
GTESTS += RangeCheckTest

GTESTPROD_HOST += NamespaceMapTest
NamespaceMapTest_SRCS += NamespaceMapTest.cpp
NamespaceMapTest_LIBS_DEFAULT += opcua
NamespaceMapTest_OBJS_WIN32 += linkParser iocshIntegration
NamespaceMapTest_OBJS_WIN32 += SessionUaSdk SubscriptionUaSdk ItemUaSdk DataElementUaSdk
NamespaceMapTest_OBJS_WIN32 += RecordConnector Session Subscription
NamespaceMapTest_LIBS_WIN32 += $(EPICS_BASE_IOC_LIBS)
GTESTS += NamespaceMapTest
