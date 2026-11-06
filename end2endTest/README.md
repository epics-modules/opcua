<!-- markdownlint-disable MD013 -->
<!-- markdownlint-disable MD043 -->

# End-to-End Test Setup - opcua

This directory contains the end-to-end test suite for the EPICS OPCUA module.

## Prerequisites

The OPC UA module must be built and the EPICS environment configured:

```shell
export EPICS_BASE=/path/to/epics/base
export EPICS_HOST_ARCH=$($EPICS_BASE/startup/EpicsHostArch)
```

Required Python version >= 3.11.

Required Python packages:

```shell
python3 -m pip install pytest asyncua pyepics run-iocsh
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

For the security tests (`test_security.py`), the server is started with a security configuration
(see `ServerSecurity` in `opcua_test_server.py`):
it offers only secure endpoints (modes `Sign` and `SignAndEncrypt`, policies `Basic256Sha256` and `Aes128_Sha256_RsaOaep`),
trusts only the IOC's client certificate and checks user name/password and X.509 user certificates.
All certificates are self-signed and created for each test session by `security_certs.py`
(in pytest's temporary directory), together with the IOC's PKI directory structure.
The IOC startup script is generated for each test.

## Tests

The tests cover:

* Connection, disconnection and reconnection
* Reading and writing supported OPC UA datatypes
* Arrays
* OPC UA timestamps
* BINI read, write and ignore behaviour
* Invalid NodeIds and datatype mismatches
* Performance and repeated access
* OPC UA security (`test_security.py`):
  * Message security modes `Sign` and `SignAndEncrypt` with different security policies,
    `best` mode, reconnect of a secure session
  * Rejection of connections without security or with a policy the server does not offer
  * Certificate trust: untrusted server certificate (IOC side) and untrusted client certificate (server side)
  * User authentication with user name/password and with an X.509 user certificate,
    rejection of wrong credentials and of anonymous access

The security tests are skipped if the module was built without crypto support
(`OPEN62541_USE_CRYPTO`/`UASDK_USE_CRYPTO` not set to `YES`).

`run-iocsh` is used to start and stop the standalone IOC.
`asyncua` is used both for the test server and for direct inspection of server-side values.

## Running the Tests

From the repository root:

```shell
make
make -C end2endTest/ioc
pytest -v end2endTest
```

The tests restrict Channel Access to `127.0.0.1` (unless `EPICS_CA_ADDR_LIST` /
`EPICS_CA_AUTO_ADDR_LIST` are already set).

On Windows, the DLLs of EPICS Base and of the OPC UA client library
(and its dependencies) must be found through `PATH`.

To run only the security tests:

```shell
pytest -v end2endTest/test_security.py
```

## References

* [asyncua](https://github.com/FreeOpcUa/opcua-asyncio)
* [pytest](https://docs.pytest.org/en/stable/)
* [run-iocsh](https://e3.pages.ess.eu/run-iocsh/)


