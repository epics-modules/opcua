TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += opcua

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install opcua.dbd into <top>/dbd
DBD += opcua.dbd

# specify all source files to be compiled and added to the library
opcua_SRCS += devOpcua.cpp
opcua_SRCS += RecordConnector.cpp
opcua_SRCS += Session.cpp
opcua_SRCS += Subscription.cpp
opcua_SRCS += Item.cpp

opcua_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================================
# link to Unified Automation SDK

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

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

#==================================================
# Unified Automation SDK libraries - INSTALL mode

ifeq ($(UASDK_DEPLOY_MODE),INSTALL)
build: $(EXTLIB_INSTALLS)

$(INSTALL_LIB)/$(LIB_PREFIX)%$(LIB_SUFFIX) : $(UASDK_DIR)/$(LIB_PREFIX)%$(LIB_SUFFIX)
        $(ECHO) "Installing library $@ from Unified Automation SDK"
        $(INSTALL) -d -m 444 $< $(@D)

$(INSTALL_LIB)/$(SHRLIB_PREFIX)%$(SHRLIB_SUFFIX) : $(UASDK_DIR)/$(SHRLIB_PREFIX)%$(SHRLIB_SUFFIX)
        $(ECHO) "Installing shared library $@ from Unified Automation SDK"
        $(INSTALL) -d -m 555 $< $(@D)
endif
