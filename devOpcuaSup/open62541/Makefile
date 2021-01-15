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
USR_INCLUDES += -I$(OPCUA)/open62541 -I$(OPEN62541)/include -I$(OPEN62541)/src_generated
USR_DBDFLAGS += -I $(OPCUA)/open62541
USR_LDFLAGS += -l open62541 -L$(OPEN62541)/lib64 -L$(OPEN62541)/lib -L$(OPEN62541)/bin

opcua_SRCS += Session.cpp
opcua_SRCS += SessionUaSdk.cpp
opcua_SRCS += Subscription.cpp
opcua_SRCS += SubscriptionUaSdk.cpp
opcua_SRCS += ItemUaSdk.cpp
opcua_SRCS += DataElementUaSdk.cpp

DBD_INSTALLS += opcua.dbd

#CFG += RULES_OPCUA
#CFG += CONFIG_OPCUA

#EXPANDVARS += OPEN62541
#EXPANDVARS += OPEN62541_DEPLOY_MODE
#EXPANDVARS += OPEN62541_DIR
#EXPANDVARS += OPEN62541_USE_CRYPTO
#EXPANDVARS += OPEN62541_USE_XMLPARSER
