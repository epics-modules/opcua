src := .

CFLAGS  := -std=c99 -I$(src) -D _BSD_SOURCE
LDFLAGS := -lpthread

all: opcuaTestServer

C_SRCS := open62541.c opcuaTestNodeSet.c opcuaTestServer.c 
C_OBJS   := $(addsuffix .o,$(basename $(C_SRCS)))

opcuaTestServer: $(C_SRCS) 
	$(CC) $(CFLAGS) $(LDFLAGS) $(C_SRCS) -o $@

clean:
	$(RM) -f *.o .depend opcuaTestServer

.PHONY: clean all 
