# End-to-End Test Setup - opcua

This directory contains the end-to-end test suite for the EPICS OPC UA module.

## Prerequisites

The OPC UA module must be built and the EPICS environment configured:

```shell
export EPICS_BASE=/path/to/epics/base
export EPICS_HOST_ARCH=$($EPICS_BASE/startup/EpicsHostArch)
```

Required Python packages:

```shell
python3 -m pip install pytest asyncua p4p run-iocsh
```

Build the standalone test IOC:

```shell
make -C end2endTest/ioc
```

## Test Setup

The suite consists of:

* A Python OPC UA test server implemented with `asyncua`
* A standalone EPICS IOC linked directly against the OPC UA module
* Pytest-based end-to-end tests

The OPC UA server is started and stopped automatically by the tests and listens on:

```text
opc.tcp://localhost:4840
```

The server provides variables covering the supported scalar OPC UA types, arrays, timestamps, monitoring, BINI behaviour, writable and read-only nodes, and negative test cases.

## Tests

The tests cover:

* Connection, disconnection and reconnection
* Reading and writing supported OPC UA datatypes
* PVAccess access using `p4p`
* Monitoring behaviour
* Arrays
* OPC UA timestamps
* BINI read, write and ignore behaviour
* Invalid NodeIds and datatype mismatches
* Performance and repeated access

`run-iocsh` is used to start and stop the standalone IOC.

`asyncua` is used both for the test server and for direct inspection of server-side values.

## Running the Tests

From the repository root:

```shell
make
make -C end2endTest/ioc
pytest -v end2endTest
```

To display IOC and server output:

```shell
pytest -v -s end2endTest
```

Run a subset of tests:

```shell
pytest -v end2endTest -k TestConnectionTests
```

Run an individual test:

```shell
pytest -v end2endTest -k test_stop_and_restart_server
```

## References

* [asyncua](https://github.com/FreeOpcUa/opcua-asyncio)
* [pytest](https://docs.pytest.org/en/stable/)
* [run-iocsh](https://e3.pages.ess.eu/run-iocsh/)
* [p4p](https://github.com/epics-base/p4p)

