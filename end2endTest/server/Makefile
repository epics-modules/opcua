src := .

CFLAGS  := -std=c99 -I$(src) -D _BSD_SOURCE -D UA_ENABLE_AMALGAMATION
LDFLAGS := -lpthread

C_SRCS   := open62541.c opcuaTestNodeSet.c opcuaTestServer.c
C_OBJS   := $(addsuffix .o,$(basename $(C_SRCS)))

PYTHON := python3
XML_SRCS := xml/opcuaTestServer.NodeSet2.xml
OPEN62541_DIR ?= /opt/open62541
NS_COMP_DIR   := $(OPEN62541_DIR)/tools/nodeset_compiler
SCHEMA_DIR    := $(OPEN62541_DIR)/deps/ua-nodeset/Schema
NS_COMP       := $(NS_COMP_DIR)/nodeset_compiler.py
SCHEMA        := $(SCHEMA_DIR)/Opc.Ua.NodeSet2.xml

all: opcuaTestServer

opcuaTestServer: $(C_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(C_SRCS) -o $@

nodeset: $(XML_SRCS)
	$(PYTHON) $(NS_COMP) --types-array=UA_TYPES --existing $(SCHEMA)  --xml $(XML_SRCS) xml/opcuaTestNodeSet

clean:
	$(RM) -f *.o .depend opcuaTestServer xml/*.c xml/*.h

.PHONY: clean all
