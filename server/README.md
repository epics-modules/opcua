Simulation server - open62541
======
This directory contains the sources for compiling an OPC UA server based on open62541 [1].

The server configuration currently consists of a single variable ``0:\\\\Test-01`` that is simulated
as an increasing ramp value, that increments by 1 every second. The limits of the ramp are
constrained between 0 and 1000. The variable is defined in the source file [\(opcuaTestNodeSet.c\)](test/server/opcuaTestNodeSet.c).

The amalgamated source files for the server (open62541.c/.h) are provided in this directory. See [2]
for more information on building the open62541 sources with amalgamation.

## Build the server
```
make
```

## Run the server
```
./opcuaTestServer
```

## Connecting to the server
By default, the server listens for connections on ``opc.tcp://localhost:4840`` and the simulated
signals are available in OPC UA namespace 2.

## References
[1] https://open62541.org/

[2 https://open62541.org/doc/current/building.html#detailed-sdk-features
