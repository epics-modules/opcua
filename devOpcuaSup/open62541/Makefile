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
#USR_INCLUDES += -I$(OPCUA)/open62541
#USR_DBDFLAGS += -I $(OPCUA)/open62541

# Find open62541 library and header files
# This should work with installed (typically to /usr/local) as well as not installed open62541
USR_INCLUDES += $(addprefix -I$(OPEN62541)/,include plugins/include arch build/src_generated)
LIB_LIBS += open62541
open62541_DIR += $(if $(filter %64,$(ARCH_CLASS)),$(OPEN62541)/lib64) $(OPEN62541)/lib $(OPEN62541)/build/bin

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
