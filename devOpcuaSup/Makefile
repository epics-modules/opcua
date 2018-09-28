#*************************************************************************
# Copyright (c) 2018 ITER Organization.
# This module is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Author: Ralph Lange <ralph.lange@gmx.de>
#
# based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>

TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

OPCUA = $(TOP)/devOpcuaSup
SRC_DIRS += $(OPCUA)

#==================================================
# build a support library

LIBRARY_IOC += opcua

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord

DBD_INSTALLS += devOpcua.dbd

# Generic sources and interfaces
opcua_SRCS += devOpcua.cpp
opcua_SRCS += iocshIntegration.cpp
opcua_SRCS += Subscription.cpp
opcua_SRCS += Item.cpp
opcua_SRCS += DataElement.cpp
opcua_SRCS += RecordConnector.cpp
opcua_SRCS += linkParser.cpp

opcua_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================================
# Implementations for different client libraries
# are in separate directories, added by reading
# Makefile snippets

# Unified Automation C++ Based OPC UA Client SDK
ifdef UASDK
include $(OPCUA)/UaSdk/Makefile
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifdef UASDK
include $(OPCUA)/UaSdk/RULES
endif
