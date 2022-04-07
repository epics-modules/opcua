Simulation server - open62541
======
This directory contains the sources for compiling an OPC UA server based on open62541 [1].

The server configuration currently consists a number of variables. The variables are defined in the XML file source file
[\(opcuaTestServer.NodeSet2.xml\)](test/server/xml/opcuaTestServer.NodeSet2.xml) and compiled into the C-source file [\(opcuaTestNodeSet.c\)](test/server/opcuaTestNodeSet.c).

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

## Compiling the NodeSet
A default, pre-compiled NodeSet is provided with the test suite in the source file [\(opcuaTestNodeSet.c\)](test/server/opcuaTestNodeSet.c).
If, however, you wish to modify the nodeset, you can recompile it using XML Nodeset Compiler [3] - a python utility for compiling NodeSet2.xml
files into C-sources for use with an open62541 server.
A target (nodeset) is provided in the Makefile to simpilify this process.

**NOTE**: You must have the open62541 source repository available on your system. By default, the Makefile expects the sources at
/opt/open62541. You can override this path, by setting the variable OPEN62541_DIR:

```
OPEN62541_DIR=/path/to/open62541 make nodeset
```

## References
[1] https://open62541.org/

[2] https://open62541.org/doc/current/building.html#detailed-sdk-features

[3] https://open62541.org/doc/current/nodeset_compiler.html
