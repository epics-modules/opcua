#*************************************************************************
# Copyright (c) 2018 ITER Organization.
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

opcua_SRCS += Session.cpp
opcua_SRCS += SessionUaSdk.cpp
opcua_SRCS += SubscriptionUaSdk.cpp
opcua_SRCS += ItemUaSdk.cpp
opcua_SRCS += DataElementUaSdk.cpp
opcua_SRCS += iocshIntegrationUaSdk.cpp

DBD_INSTALLS += opcuaUaSdk.dbd
DBD_INSTALLS += opcua.dbd

UASDK_LIBS = uabase uaclient uapki uastack xmlparser
USR_INCLUDES += $(foreach module, $(UASDK_LIBS), -I$(UASDK)/include/$(module))

ifeq ($(UASDK_DEPLOY_MODE),SYSTEM)
USR_SYS_LIBS += $(UASDK_LIBS)
endif

ifeq ($(UASDK_DEPLOY_MODE),PROVIDED)
define UA_template
  USR_LIBS += $(1)
  $(1)_DIR = $(UASDK_DIR)
endef
$(foreach lib, $(UASDK_LIBS), $(eval $(call UA_template,$(lib))))
endif

ifeq ($(UASDK_DEPLOY_MODE),INSTALL)
EXTLIB_INSTALLS += $(addprefix $(INSTALL_LIB)/, \
    $(notdir $(wildcard $(foreach lib, $(UASDK_LIBS), \
    $(UASDK_DIR)/$(LIB_PREFIX)$(lib)$(LIB_SUFFIX) ))))
    ifeq ($(SHARED_LIBRARIES),YES)
EXTLIB_INSTALLS += $(addprefix $(INSTALL_LIB)/, \
    $(notdir $(wildcard $(foreach lib, $(UASDK_LIBS), \
    $(UASDK_DIR)/$(SHRLIB_PREFIX)$(lib)$(SHRLIB_SUFFIX) ))))
    endif
endif
