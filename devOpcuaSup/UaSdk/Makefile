#*************************************************************************
# Copyright (c) 2018-2020 ITER Organization.
# This module is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Author: Ralph Lange <ralph.lange@gmx.de>
#
# based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
# and example code from the Unified Automation C++ Based OPC UA Client SDK

# This is a Makefile fragment, see devOpcuaSup/Makefile.

#==================================================
# Implementation using Unified Automation SDK

SRC_DIRS += $(OPCUA)/UaSdk
USR_INCLUDES += -I$(OPCUA)/UaSdk
USR_DBDFLAGS += -I $(OPCUA)/UaSdk

ifeq ($(UASDK_USE_CRYPTO),YES)
USR_CXXFLAGS += -DHAS_SECURITY
endif

opcua_SRCS += Session.cpp
opcua_SRCS += SessionUaSdk.cpp
opcua_SRCS += Subscription.cpp
opcua_SRCS += SubscriptionUaSdk.cpp
opcua_SRCS += ItemUaSdk.cpp
opcua_SRCS += DataElementUaSdk.cpp

DBD_INSTALLS += opcua.dbd

CFG += RULES_OPCUA
CFG += CONFIG_OPCUA

EXPANDVARS += UASDK
EXPANDVARS += UASDK_DEPLOY_MODE
EXPANDVARS += UASDK_DIR
EXPANDVARS += UASDK_USE_CRYPTO
EXPANDVARS += UASDK_USE_XMLPARSER
