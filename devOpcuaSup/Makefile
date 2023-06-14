#*************************************************************************
# Copyright (c) 2018 ITER Organization.
# This module is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Author: Ralph Lange <ralph.lange@gmx.de>
#
# based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
# and code by Michael Davidsaver <mdavidsaver@ospreydcs.com>

TOP=..

include $(TOP)/configure/CONFIG
include CONFIG_OPCUA
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

OPCUA = $(TOP)/devOpcuaSup
SRC_DIRS += $(OPCUA)

ifeq ($(shell git rev-parse --is-inside-work-tree),true)
  EPICS_OPCUA_GIT_COMMIT := $(shell git describe --long --dirty)
else
  EPICS_OPCUA_GIT_COMMIT := -
endif

#==================================================
# build a support library

LIBRARY_IOC += opcua

# generate headers and html from .dbd
DBDINC += opcuaItemRecord
DBDINC += menuDefAction.h

DBD_INSTALLS += $(COMMON_DIR)/devOpcua.dbd

COMMON_ASSEMBLIES += devOpcua.dbd

ifdef BASE_3_16
devOpcua.dbd_SNIPPETS += ../10_devOpcuaInt64.dbd
endif
devOpcua.dbd_SNIPPETS += ../20_devOpcuaAll.dbd

# Generic sources and interfaces
opcua_SRCS += devOpcua.cpp
opcua_SRCS += iocshIntegration.cpp
opcua_SRCS += RecordConnector.cpp
opcua_SRCS += linkParser.cpp
opcua_SRCS += opcuaItemRecord.cpp

opcua_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += devOpcuaVersion.h
INC += devOpcuaVersionNum.h

HTMLS += menuDefAction.html

#==================================================
# Implementations for different client libraries
# are in separate directories, added by reading
# Makefile fragments

SUPPORTED_SDKS = UASDK OPEN62541

ifneq ($(words $(foreach SDK,$(SUPPORTED_SDKS),$($(SDK)))),1)
$(warning Set your OPC UA client library in CONFIG_SITE.local)
$(warning Define exactly one of these variables: $(SUPPORTED_SDKS))
$(foreach SDK,$(SUPPORTED_SDKS),$(warning $(SDK) = $($(SDK))))
$(error)
endif

# Unified Automation C++ Based OPC UA Client SDK
ifdef UASDK
include $(OPCUA)/UaSdk/Makefile.config
endif

# open62541 project
ifdef OPEN62541
include $(OPCUA)/open62541/Makefile.config
endif

# Module versioning
EXPANDVARS += EPICS_OPCUA_MAJOR_VERSION
EXPANDVARS += EPICS_OPCUA_MINOR_VERSION
EXPANDVARS += EPICS_OPCUA_MAINTENANCE_VERSION
EXPANDVARS += EPICS_OPCUA_DEVELOPMENT_FLAG
EXPANDVARS += EPICS_OPCUA_GIT_COMMIT

EXPANDFLAGS += $(foreach var,$(EXPANDVARS),-D$(var)="$(strip $($(var)))")

# Shared library ABI version
SHRLIB_VERSION = $(EPICS_OPCUA_MAJOR_VERSION).$(EPICS_OPCUA_MINOR_VERSION)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# Unified Automation C++ Based OPC UA Client SDK
ifdef UASDK
include $(OPCUA)/UaSdk/Makefile.rules
endif

# open62541 project
ifdef OPEN62541
include $(OPCUA)/open62541/Makefile.rules
endif

# Can't use EXPAND as generated headers must appear
# in O.Common, but EXPAND emits rules for O.$(T_A)
../O.Common/devOpcuaVersionNum.h: ../devOpcuaVersionNum.h@ redo-version
	@$(RM) $@
	$(EXPAND_TOOL) $(EXPANDFLAGS) $($@_EXPANDFLAGS) $< $@

.PHONY: redo-version
