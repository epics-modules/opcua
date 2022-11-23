# Test Setup - opcua
This directory contains the sources for the automatic testing of the e3-opcua module.

## Prerequisites
In order to run the test suite, you must install the following:

 * python3
 * libfaketime

On CentOS 7, run the following:

```
sudo yum install -y python3 libfaketime
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
A test IOC is provided that translates the OPC UA variables from the test server.
The following records are defined:

| Record         | EPICS Type | OPC-UA Type | Record            | EPICS Type | OPC-UA Type |
|----------------|------------|-------------|-------------------|------------|-------------|
| TstRamp        | ai         | Double      |                   |            |             |
| VarCheckBool   | bi         | Bool        | VarCheckBoolOut   | bo         | Bool        |
| VarCheckSByte  | ai         | SByte       | VarCheckSByteOut  | ao         | SByte       |
| VarCheckByte   | ai         | Byte        | VarCheckByteOut   | ao         | Byte        |
| VarCheckUInt16 | ai         | Uint16      | VarCheckUInt16Out | ao         | Uint16      |
| VarCheckInt16  | ai         | Int16       | VarCheckInt16Out  | ao         | Int16       |
| VarCheckUInt32 | ai         | Uint32      | VarCheckUInt32Out | ao         | Uint32      |
| VarCheckInt32  | ai         | Int32       | VarCheckInt32Out  | ao         | Int32       |
| VarCheckUInt64 | ai         | Uint64      | VarCheckUInt64Out | ao         | Uint64      |
| VarCheckInt64  | ai         | Int64       | VarCheckInt64Out  | ao         | Int64       |
| VarCheckString | stringin   | String      | VarCheckStringOut | stringout  | String      |
| VarCheckFloat  | ai         | Float       | VarCheckFloatOut  | ao         | Float       |
| VarCheckDouble | ai         | Double      | VarCheckDoubleOut | ao         | Double      |

A secondary IOC is provided for use with the negative tests, consisting of two records:

 * BadVarName    - an analog input that specifies an incorrect OPC-UA variable name
 * VarNotBoolean - a binary input that specifies an OPC-UA variable of float datatype

Startup scripts and database files are provided in the
cmd/ and db/ subdirectories.

## Python Test Files
The pytest framework [2] is used to implement the test cases. Individual test cases are provided
as python functions (defs) in [\(opcua_test_cases.py\)](test/opcua_test_cases.py). Under the hood,
run_iocsh [3] and pyepics [4] are used for communication with the test IOC.

To add a new test case, simply add a new funtion (def) to [\(opcua_test_cases.py\)](test/opcua_test_cases.py),
ensuring that the function name begins with the prefix ``test_``

### Test Classes and Test Cases

The test points are split into four classes:

#### Connection tests (TestConnectionTests)

 1. **_test_connect_disconnect_**: start and stop the test IOC 5 times. Parse the IOC output,
      and check it connects and disconnects to the OPC-UA server successfully.

 2. **_test_connect_reconnect_**: Start the server, start the IOC. Stop the server,
      check for appropriate messaging. Start the server, check that the IOC reconnects.

 3. **_test_no_connection_**: Start an IOC with no server running. Check the module reports appropriately.

 4. **_test_shutdown_on_ioc_reboot_**: Start the server. Start an IOC and ensure connection
      is made to the server. Shutdown the IOC and endure that the subscriptions and sessions are
      cleanly disconnected.


#### Variable tests (TestVariableTests)

 1. **_test_server_status_**: Check the informational values provided by the server are being
      translated via the module.

 2. **_test_variable_pvget_**: Start the test IOC and use pvget to read the ``TstRamp`` PV
      value multiple times (every second). Check that it is incrementing as a ramp.

 3. **_test_read_variable_**: Read the deafult value of a variable from the opcua server and
      check it matches the expected value. Parametrised for all supported datatypes
      (boolean, sbyte, byte, int16, uint16, int32, uint32, int64, uint64, float, double, string.)

 4. **_test_write_variable_**: Write a known value to the opcua server via the output PV linked
      to the variable. Read back via the input PV and check the values match. Parametrised for all
      supported datatypes.
      (boolean, sbyte, byte, int16, uint16, int32, uint32, int64, uint64, float, double, string.)

 5. **_test_timestamps_**:  Start the test server in a shell session with with a fake time in
      the past, using libfaketime [5]. Check that the timestamp for the PV read matches the
      known fake time given to the server. If they match, the OPCUA EPICS module is correctly
      pulling the timestamps from the OPCUA server (and not using a local timestamp).


#### Performance tests (TestPerformanceTests)

 1. **_test_write_performance_**: Write 5000 variable values and measure time and memory
      consumption before and after. Repeat 10 times

 2. **_test_read_performance_**: Read 5000 variable values and measure time and memory
      consumption before and after. Repeat 10 times


#### Negative tests (TestNegativeTests)

 1. **_test_no_server_**: Start an OPC-UA IOC with no server running.
      Check the module reports this correctly.

 2. **_test_bad_var_name_**:  Specify an incorrect variable name in a db record.
      Start the IOC and verify a sensible error is displayed.

 3. **_test_wrong_datatype_**: Specify an incorrect record type for an OPC-UA variable.
      Binary input record for a float datatype.


### Running the test suite
You can run the test suite from the root of the repository wuth the following command:
```
pytest -v
```

To view the stdout output from the tests in real-time, you can provide the ``-s`` flag:
```
pytest -v -s
```

To run all tests in a class:
```
pytest -v -k TestConnectionTests
```

To run an individual test point:
```
pytest -v -k test_connect_disconnect
```

## References
[1] https://open62541.org/

[2] https://docs.pytest.org/en/stable/

[3] https://gitlab.esss.lu.se/ics-infrastructure/run-iocsh

[4] http://pyepics.github.io/pyepics/

[5] https://github.com/wolfcw/libfaketime
