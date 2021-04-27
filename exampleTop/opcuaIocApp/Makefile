TOP=..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = opcuaIoc

# opcuaIoc.dbd will be created and installed
DBD += opcuaIoc.dbd

# opcuaIoc.dbd will be made up from these files:
opcuaIoc_DBD += base.dbd

# Include dbd files from all support applications:
opcuaIoc_DBD += opcua.dbd

# Add all the support libraries needed by this IOC
opcuaIoc_LIBS += opcua $(UASDK_LIBS)
opcuaIoc_SYS_LIBS_Linux += $(UASDK_SYS_LIBS_Linux)

# opcuaIoc_registerRecordDeviceDriver.cpp derives from opcuaIoc.dbd
opcuaIoc_SRCS += opcuaIoc_registerRecordDeviceDriver.cpp
opcuaIoc_SRCS += opcuaIocMain.cpp

# Finally link to the EPICS Base libraries
opcuaIoc_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
