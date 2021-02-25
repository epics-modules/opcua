# Test Setup - opcua
This directory contains the sources for the automatic testing of the e3-opcua module.
It consists of three main components:

## OPC-UA Server - open62541
A simple test opcua server, created using open62541 [1]. The server configuration currently
consists of a single variable ``0:\\\\Test-01`` that is simulated as an increasing ramp value, that
increments by 1 every second. The limits of the ramp are constrained between 0 and 1000.
The variable is defined in the source file [\(opcuaTestNodeSet.c\)](test/server/opcuaTestNodeSet.c).

The server listens for connections on ``opc.tcp://localhost:4840`` and the simulated
signals are available in OPC UA namespace 2.

For further information on the server configuration, see [simulation server](test/server/README.md).

## IOC
A test IOC is provided, that consists of a single PV ``Tst-01`` that translates the OPC-UA
variable from the test server. A startup script and database file are provided in the
cmd/ and db/ subdirectories.

## Python Test Files
The pytest framework [2] is used to implement the test cases. Individual test cases are provided
as python functions (defs) in [\(opcua-test-cases.py\)](test/opcua-test-cases.py). Under the hood, run_iocsh [3] and pyepics [4] are
used for communication with the test IOC.

The test cases provided are:

 1. **_test_connect_disconnect_**: start and stop the test IOC 5 times. Parse the IOC output, and check it
   connects and disconnects to the OPC-UA server successfully.

 2. **_test_variable_pvget_**: Start the test IOC and use pvget to read the 'Tst-01' PV value multiple times (every second).
   Check that it is incrementing as a ramp.

## References
[1] https://open62541.org/

[2] https://docs.pytest.org/en/stable/

[3] https://gitlab.esss.lu.se/ics-infrastructure/run-iocsh

[4] http://pyepics.github.io/pyepics/
