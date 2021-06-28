TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install) into <top>/db
# databases, templates, substitutions like this
#DB += xxx.db

DB += UaDemoServer-server.db

DB += Demo.Dynamic.Arrays.db
DB += Demo.Dynamic.ArraysE7.db
DB += Demo.Dynamic.Scalar.db
DB += Demo.Dynamic.ScalarE7.db

DB += Demo.Static.Arrays.db
DB += Demo.Static.ArraysE7.db
DB += Demo.Static.Scalar.db
DB += Demo.Static.ScalarE7.db

DB += Demo.WorkOrder.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

