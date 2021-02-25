Simulation server - open62541
======
This directory contains the sources for compiling an OPC UA server based on open62541 [1].

The server is configured to provide a subset (one HV and on LV substation) of signals via OPC UA.
These signals are defined by the SimulationNodes.NodeSet2.xml file in the xml directory. This XML 
source file has then be compiled into the C source files (cfhvmsNodeSet.c/.h), that is used to build
the test server executable.

The XML file is compiled into C using XML Nodeset Compiler [2], a python utility provided as part
of the open62541 sources.

The amalgamated source files for the server (open62541.c/.h) are provided in this directory. See [3]
for more information on building the open62541 sources with amalgamation.

## Build the server
```
make
```

## Run the server
```
./cfhvmsTestServer
```

## Connecting to the server
By default, the server listens for connections on ``opc.tcp://localhost:4840`` and the simulated 
signals are available in OPC UA namespace 2.

## References
[1] https://open62541.org/ 

[2] https://open62541.org/doc/current/nodeset_compiler.html

[3] https://open62541.org/doc/current/building.html#detailed-sdk-features
