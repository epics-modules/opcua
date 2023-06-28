TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# Unit tests may use different flags than the library build (e.g., for coverage)
# To avoid mix-ups, the specific library classes are recompiled here for their tests
# and the test executables are not linked against libopcua.

# Windows builds define NOMINMAX (macros min, max clash with numeric_limits<>)
USR_CXXFLAGS_WIN32 += -DNOMINMAX

DEVSUP_SRC = $(TOP)/devOpcuaSup

TESTSRC = $(TOP)/unitTestApp/src
CLIENT_SRC = $(DEVSUP_SRC)/$(CLIENT_SUBDIR)

# Add client specific tests
SRC_DIRS += $(TESTSRC)/$(CLIENT_SUBDIR)
# Location of the library classes under test
SRC_DIRS += $(DEVSUP_SRC) $(CLIENT_SRC)
USR_INCLUDES += -I$(DEVSUP_SRC) -I$(CLIENT_SRC)

ifeq ($(CLIENT)_USE_CRYPTO),YES)
USR_CXXFLAGS += -DHAS_SECURITY
endif
ifeq ($($(CLIENT)_USE_XMLPARSER),YES)
USR_CXXFLAGS += -DHAS_XMLPARSER
USR_INCLUDES_Linux += -I/usr/include/libxml2
ifneq ($(filter -D_MINGW,$(OP_SYS_CPPFLAGS)),)
USR_INCLUDES += -I$(MSYSTEM_PREFIX)/include/libxml2
USR_SYS_LIBS += xml2
endif
endif

# Link explicitly against locally compiled library objects
OPCUA_OBJS += linkParser iocshIntegration $($(CLIENT)_OPCUA_OBJS)
OPCUA_OBJS += RecordConnector Session Subscription

#==================================================
# Build tests executables

GTESTPROD_HOST += UpdateTest
UpdateTest_SRCS += UpdateTest.cpp
GTESTS += UpdateTest

GTESTPROD_HOST += UpdateQueueTest
UpdateQueueTest_SRCS += UpdateQueueTest.cpp
GTESTS += UpdateQueueTest

GTESTPROD_HOST += RequestQueueBatcherTest
RequestQueueBatcherTest_SRCS += RequestQueueBatcherTest.cpp
GTESTS += RequestQueueBatcherTest

GTESTPROD_HOST += RegistryTest
RegistryTest_SRCS += RegistryTest.cpp
GTESTS += RegistryTest

GTESTPROD_HOST += LinkParserTest
LinkParserTest_SRCS += LinkParserTest.cpp
LinkParserTest_LIBS += $($(CLIENT)_LIBS) $(EPICS_BASE_IOC_LIBS)
LinkParserTest_SYS_LIBS_Linux += $(OPCUA_SYS_LIBS_Linux)
LinkParserTest_OBJS += $(OPCUA_OBJS)
GTESTS += LinkParserTest

GTESTPROD_HOST += ElementTreeTest
ElementTreeTest_SRCS += ElementTreeTest.cpp
ElementTreeTest_LIBS += $($(CLIENT)_LIBS) $(EPICS_BASE_IOC_LIBS)
ElementTreeTest_SYS_LIBS_Linux += $(OPCUA_SYS_LIBS_Linux)
ElementTreeTest_OBJS += $(OPCUA_OBJS)
GTESTS += ElementTreeTest

#==================================================
# Tests for different client libraries
# are in separate directories, added by reading
# Makefile fragments

# Include client specific Makefile fragment (config part)
ifdef T_A
include $(TESTSRC)/$(CLIENT_SUBDIR)/Makefile.config
endif

USR_LIBS += Com

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifdef UASDK

# Try to fight DLL Hell by putting SSL DLLs next to the binaries

SSLLIB_INSTALLS += $(foreach lib, $(UASDK_SSL_LIBS), \
    $(SHRLIB_PREFIX)$(lib)$(SHRLIB_SUFFIX_BASE) )

$(GTESTLISTS) : $(SSLLIB_INSTALLS)

$(SHRLIB_PREFIX)%$(SHRLIB_SUFFIX_BASE) : $(UASDK_SHRLIB_DIR)/$(SHRLIB_PREFIX)%$(SHRLIB_SUFFIX_BASE)
	@$(ECHO) "Installing shared library $@ from Unified Automation SDK"
	$(INSTALL) -d -m 555 $< $(@D)

endif
