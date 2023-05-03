#*************************************************************************
# Copyright (c) 2020 Paul Scherrer Institut.
# This module is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Author: Dirk Zimoch <dirk.zimoch@psi.ch>
#
# based on the interface for the Unified Automation OPC UA Client SDK
# by Ralph Lange <ralph.lange@gmx.de>

# This is a Makefile fragment, see devOpcuaSup/Makefile.

#=========================================================
# Implementation using open62541 (https://open62541.org/)

SRC_DIRS += $(OPCUA)/open62541

# The build directoy of open62541, named "build" by default
# but look for dirs with EPICS $(T_A) in the name, too.
ifndef OPEN62541_BUILD_DIR
OPEN62541_BUILD_DIR := $(notdir $(firstword $(wildcard $(OPEN62541)/build/$(T_A) $(OPEN62541)/build_$(T_A) $(OPEN62541)/build-$(T_A) $(OPEN62541)/$(T_A) $(OPEN62541)/build)))
endif

# Find open62541 library and header files
# This should work with installed (typically to /usr/local) as well as not installed open62541
USR_INCLUDES += $(addprefix -I$(OPEN62541)/,. include plugins/include arch $(OPEN62541_BUILD_DIR:%=%/src_generated))
LIB_LIBS += open62541
open62541_DIR += $(OPEN62541)/$(OPEN62541_BUILD_DIR)/bin/Release $(OPEN62541)/$(OPEN62541_BUILD_DIR)/bin $(if $(filter %64,$(ARCH_CLASS)),$(OPEN62541)/lib64) $(OPEN62541)/lib
USR_SYS_LIBS_Linux += crypto

opcua_SRCS += Session.cpp
opcua_SRCS += SessionOpen62541.cpp
opcua_SRCS += Subscription.cpp
opcua_SRCS += SubscriptionOpen62541.cpp
opcua_SRCS += ItemOpen62541.cpp
opcua_SRCS += DataElementOpen62541.cpp

DBD_INSTALLS += opcua.dbd

#CFG += RULES_OPCUA
#CFG += CONFIG_OPCUA

#EXPANDVARS += OPEN62541
#EXPANDVARS += OPEN62541_DEPLOY_MODE
#EXPANDVARS += OPEN62541_DIR
#EXPANDVARS += OPEN62541_USE_CRYPTO
#EXPANDVARS += OPEN62541_USE_XMLPARSER
