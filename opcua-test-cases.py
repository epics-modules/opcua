from epics import PV
from time import sleep
from run_iocsh import IOC
from os import environ
import pytest
import subprocess


class opcuaTestHarness:
    def __init__(self):

        # Get values from the environment
        self.EPICS_BASE = environ.get("EPICS_BASE")
        self.REQUIRE_VERSION = environ.get("E3_REQUIRE_VERSION")
        self.MOD_VERSION = environ.get("E3_MODULE_VERSION")
        if self.MOD_VERSION is None:
            self.MOD_VERSION = "0.8.0"
        self.TEMP_CELL_PATH = environ.get("TEMP_CELL_PATH")
        if self.TEMP_CELL_PATH is None:
            self.TEMP_CELL_PATH = "cellMods"

        # run-iocsh parameters
        self.IOCSH_PATH = (
            f"{self.EPICS_BASE}/require/{self.REQUIRE_VERSION}/bin/iocsh.bash"
        )

        self.TestArgs = [
            "-l",
            self.TEMP_CELL_PATH,
            "-r",
            f"opcua,{self.MOD_VERSION}",
        ]

        self.cmd = "test/cmds/test_pv.cmd"

        # Default IOC
        self.IOC = IOC(
            *self.TestArgs,
            self.cmd,
            ioc_executable=self.IOCSH_PATH,
        )

        # test sleep time ins seconds
        self.sleepTime = 10

        # Test server
        self.testServer = "test/server/opcuaTestServer"

        # Message catalog
        self.connectMsg = (
            "OPC UA session OPC1: connection status changed"
            + " from Connected to Disconnected"
        )
        self.reconnectMsg = (
            "OPC UA session OPC1: connection status changed"
            + " from ConnectionErrorApiReconnect to NewSessionCreated"
        )
        self.reconnectMsg1 = (
            "OPC UA session OPC1: connection status changed"
            + " from NewSessionCreated to Connected"
        )
        self.noConnectMsg = (
            "OPC UA session OPC1: connection status changed"
            + " from Disconnected to ConnectionErrorApiReconnect"
        )

    def start_server(self):
        self.serverProc = subprocess.Popen(self.testServer, shell=False)
        print("\nOpened server with pid = %s" % self.serverProc.pid)

    def stop_server(self):
        print("\nClosing server with pid = %s" % self.serverProc.pid)
        self.serverProc.terminate()


# Standard test fixture
@pytest.fixture()
def test_inst():
    """Instantiate test harness, start the server,
    yield the harness handle to the test,
    close the server on test end / failure
    """
    test_inst = opcuaTestHarness()
    test_inst.start_server()
    yield test_inst
    test_inst.stop_server()


# Positive Test Cases
def test_connect_disconnect(test_inst):
    """
    Connect and disconnect to the OPC-UA test server, and
    check that there are no errors
    """

    ioc = test_inst.IOC
    nRuns = 5

    for i in range(0, nRuns):
        # Start IOC, and check it is running
        ioc.start()
        assert ioc.is_running()

        ioc.exit()
        assert not ioc.is_running()

        # Grab ioc output
        ioc.check_output()
        output = ioc.outs

        # Parse for OPC-UA connection message
        assert (
            output.find(test_inst.connectMsg) >= 0
        ), "%d: Failed to find disconnect message in output\n%s" % (i, output)
        print(output)


def test_connect_reconnect(test_inst):
    """
    Start the server, start the IOC. Stop the server, check
    for appropriate messaging. Start the server, check that
    the IOC reconnects.
    """
    ioc = test_inst.IOC

    nRuns = 5

    for i in range(0, nRuns):
        ioc.start()
        assert ioc.is_running()

        test_inst.stop_server()
        assert ioc.is_running()

        sleep(test_inst.sleepTime)

        test_inst.start_server()
        assert ioc.is_running()

        sleep(test_inst.sleepTime)

        ioc.exit()
        assert not ioc.is_running()

    # Grab ioc output
    ioc.check_output()
    output = ioc.outs
    print(output)

    # Parse for OPC-UA connection message
    assert (
        output.find(test_inst.reconnectMsg) >= 0
    ), "%d: Failed to find reconnect message in output\n%s" % (i, output)
    assert (
        output.find(test_inst.reconnectMsg1) >= 0
    ), "%d: Failed to find reconnect message 1 in output\n%s" % (i, output)


def test_no_connection(test_inst):
    """
    Start an IOC with no server running. Check the module
    reports this.
    """

    ioc = test_inst.IOC

    # We don't want the server running
    test_inst.stop_server()

    # Start the IOC
    ioc.start()
    assert ioc.is_running()

    sleep(test_inst.sleepTime)

    test_inst.start_server()

    sleep(test_inst.sleepTime)

    ioc.exit()
    assert not ioc.is_running()

    # Grab ioc output
    ioc.check_output()
    output = ioc.outs
    print(output)

    i = 1
    # Parse for OPC-UA connection message
    assert (
        output.find(test_inst.noConnectMsg) >= 0
    ), "%d: Failed to find no connection message in output\n%s" % (i, output)
    assert (
        output.find(test_inst.reconnectMsg) >= 0
    ), "%d: Failed to find reconnect message in output\n%s" % (i, output)
    assert (
        output.find(test_inst.reconnectMsg1) >= 0
    ), "%d: Failed to find reconnect message 1 in output\n%s" % (i, output)


def test_variable_pvget(test_inst):
    """
    Variable on the OPCUA server increments by 1 each second
    Sample the value using pvget once a second and check the
    value is incrmenting.
    """

    with IOC(
        *test_inst.TestArgs,
        test_inst.cmd,
        ioc_executable=test_inst.IOCSH_PATH,
    ):
        # PV name
        pvName = "Tst-01"
        pv = PV(pvName)

        # Test parameters
        minVal = 0  # Ramp min, as defined by server
        maxVal = 1000  # Ramp max, as defined by server
        captureLen = 5  # Number of samples to capture
        captureIncr = 1  # Time increment in seconds

        res = [int(0)] * captureLen
        resCheck = [int(0)] * captureLen

        for i in range(0, captureLen):
            res[i] = int(pv.get())
            sleep(captureIncr)

        wrapOffset = 0
        resCheck[0] = res[0]
        # Check ramp is incrementing correctly
        for i in range(1, captureLen):
            # Handle possible wraparound from 1000 -> 0
            if res[i] == minVal:
                wrapOffset = maxVal + 1
            resCheck[i] = res[i] + wrapOffset
            assert (
                resCheck[i] == resCheck[i - 1] + captureIncr
            ), "Captured value (%d) is %d. Expected %d" % (
                i,
                res[i],
                res[i - 1] + captureIncr,
            )
