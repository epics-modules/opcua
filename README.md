# Test Setup - opcua
This directory contains the sources for the automatic testing of the e3-opcua module.

## Prerequisites
In order to run the test suite, you must install the following:

 * python3

On CentOS 7, run the following:

```
sudo yum install python3
```

And the following python modules:

 * pytest
 * pyepics
 * opcua
 * run-iocsh

You can use the following pip3 commands:

```
pip3 install pytest opcua pyepics
pip3 install run-iocsh -i https://artifactory.esss.lu.se/artifactory/api/pypi/pypi-virtual/simple
```

You must configure the EPICS environment before running the test suite. 
For the E3 environment, this requires you to ``source setE3Env.bash``.

Finally, compile the test server for use by the test suite:
```
cd test/server
make
```


## Test Suite Components

The test setup consists of three main components:

### OPC-UA Server - open62541
A simple test opcua server, created using open62541 [1]. The server configuration currently
consists of a number of variables provided for testing purposes.

The server listens for connections on ``opc.tcp://localhost:4840`` and the simulated
signals are available in OPC UA namespace 2.

For further information on the server configuration, see [simulation server](test/server/README.md).

### IOC
A test IOC is provided that translates the OPC UA variables from the test server. The following PVs are defined:

 * TstRamp
 * VarCheckBool
 * VarCheckSByte
 * VarCheckByte
 * VarCheckUInt16
 * VarCheckInt16
 * VarCheckUInt32
 * VarCheckInt32
 * VarCheckUInt64
 * VarCheckInt64
 * VarCheckString
 * VarCheckFloat
 * VarCheckDouble

A startup script and database file are provided in the
cmd/ and db/ subdirectories.

## Python Test Files
The pytest framework [2] is used to implement the test cases. Individual test cases are provided
as python functions (defs) in [\(opcua_test_cases.py\)](test/opcua_test_cases.py). Under the hood, run_iocsh [3] and pyepics [4] are
used for communication with the test IOC.

To add a new test case, simply add a new funtion (def) to [\(opcua_test_cases.py\)](test/opcua_test_cases.py), ensuring that the function name begins with the prefix ``test_``

The test cases provided are:

 1. **_test_connect_disconnect_**: start and stop the test IOC 5 times. Parse the IOC output, and check it
   connects and disconnects to the OPC-UA server successfully.

 2. **_test_connect_reconnect_**: Start the server, start the IOC. Stop the server, check for appropriate messaging.
   Start the server, check that the IOC reconnects.

 3. **_test_no_connection_**: Start an IOC with no server running. Check the module reports appropriately.

 4. **_test_server_status_**: Check the informational values provided by the server are being translated via the module.


 5. **_test_variable_pvget_**: Start the test IOC and use pvget to read the ``TstRamp`` PV value multiple times (every second).
   Check that it is incrementing as a ramp.

### Running the test suite
You can run the test suite from the root of the repository wuth the following command:
```
pytest -v test/opcua_test_cases.py
```

To view the stdout output from the tests in real-time, you can provide the ``-s`` flag:
```
pytest -v -s test/opcua_test_cases.py
```

To run an individual test point:
```
pytest -v test/opcua_test_cases.py::TestConnectionTests::test_connect_disconnect
```

## References
[1] https://open62541.org/

[2] https://docs.pytest.org/en/stable/

[3] https://gitlab.esss.lu.se/ics-infrastructure/run-iocsh

[4] http://pyepics.github.io/pyepics/
