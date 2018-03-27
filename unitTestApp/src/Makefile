TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += unitTest

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install unitTest.dbd into <top>/dbd
#DBD += unitTest.dbd

# specify all source files to be compiled and added to the library
#unitTest_SRCS += xxx

unitTest_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

