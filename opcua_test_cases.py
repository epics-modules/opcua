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

        # timeout value in seconds for pvput/pvget calls
        self.timeout = 5
        self.putTimeout = self.timeout
        self.getTimeout = self.timeout

        # test sleep time ins seconds
        self.sleepTime = 3

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

        # Server variables
        self.serverVars = [
            "open62541",
            "open62541 OPC UA Server",
            "1.2.0-29-g875d33a9",
        ]

    def start_server(self):
        self.serverProc = subprocess.Popen(self.testServer, shell=False)
        print("\nOpened server with pid = %s" % self.serverProc.pid)

    def stop_server(self):
        print("\nClosing server with pid = %s" % self.serverProc.pid)
        # Send terminate signal
        self.serverProc.terminate()
        # Wait for processes to terminate.
        self.serverProc.wait(timeout=5)

    def is_server_running(self):
        ret = self.serverProc.poll()
        if ret is None:
            return True
        else:
            return False


# Standard test fixture
@pytest.fixture(scope="function")
def test_inst():
    """
    Instantiate test harness, start the server,
    yield the harness handle to the test,
    close the server on test end / failure
    """
    test_inst = opcuaTestHarness()
    test_inst.start_server()
    yield test_inst
    test_inst.stop_server()
    assert not test_inst.is_server_running()


class TestConnectionTests:

    # Positive Test Cases
    def test_connect_disconnect(self, test_inst):
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
            ), "%d: Failed to find disconnect message\n%s" % (i, output)

            print(output)

    def test_connect_reconnect(self, test_inst):
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
        ), "%d: Failed to find reconnect message\n%s" % (i, output)
        assert (
            output.find(test_inst.reconnectMsg1) >= 0
        ), "%d: Failed to find reconnect message 1\n%s" % (i, output)

    def test_no_connection(self, test_inst):
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
        ), "%d: Failed to find no connection message\n%s" % (i, output)
        assert (
            output.find(test_inst.reconnectMsg) >= 0
        ), "%d: Failed to find reconnect message in output\n%s" % (i, output)
        assert (
            output.find(test_inst.reconnectMsg1) >= 0
        ), "%d: Failed to find reconnect message 1 in output\n%s" % (i, output)


class TestVariableTests:
    def test_server_status(self, test_inst):
        """
        Check the informational values provided by the server
        are being translated via the module
        """
        ioc = test_inst.IOC

        serverVars = [
            "OPC:ServerManufacturerName",
            "OPC:ServerProductName",
            "OPC:ServerSoftwareVersion",
        ]
        i = 0
        with ioc:
            for pvName in serverVars:
                pv = PV(pvName)
                res = pv.get(timeout=test_inst.getTimeout)
                assert res == test_inst.serverVars[i]
                i = i + 1

    def test_variable_pvget(self, test_inst):
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
            pvName = "TstRamp"
            pv = PV(pvName)

            # Test parameters
            minVal = 0  # Ramp min, as defined by server
            maxVal = 1000  # Ramp max, as defined by server
            captureLen = 5  # Number of samples to capture
            captureIncr = 1  # Time increment in seconds

            res = [int(0)] * captureLen
            resCheck = [int(0)] * captureLen

            for i in range(0, captureLen):
                res[i] = int(pv.get(timeout=test_inst.getTimeout))
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

    @pytest.mark.parametrize(
        "pvName,expectedVal",
        [
            ("VarCheckBool", True),
            ("VarCheckSByte", -128),
            ("VarCheckByte", 255),
            ("VarCheckInt16", -32768),
            ("VarCheckUInt16", 65535),
            ("VarCheckInt32", -2147483648),
            ("VarCheckUInt32", 4294967295),
            ("VarCheckInt64", -1294967296),
            ("VarCheckUInt64", "{:.16e}".format(18446744073709551615)),
            ("VarCheckFloat", -0.0625),
            ("VarCheckDouble", 0.002),
            ("VarCheckString", "TestString01"),
        ],
    )
    def test_read_variable(self, test_inst, pvName, expectedVal):
        """
        Read the deafult value of a variable from the opcua
        server and check it matches the expected value.
        Parametrised for all supported datatypes.
        """
        ioc = test_inst.IOC

        with ioc:
            pv = PV(pvName)
            res = pv.get(timeout=test_inst.getTimeout)
            # Check UInt64 with correct scientific notation
            if pvName == "VarCheckUInt64":
                res = "%.16e" % res
            # Compare
            assert res == expectedVal

    @pytest.mark.parametrize(
        "pvName,writeVal",
        [
            ("VarCheckBool", False),
            ("VarCheckSByte", 127),
            ("VarCheckByte", 128),
            ("VarCheckInt16", 32767),
            ("VarCheckUInt16", 32768),
            ("VarCheckInt32", 2147483647),
            ("VarCheckUInt32", 2147483648),
            ("VarCheckInt64", 0),
            ("VarCheckUInt64", 0),
            ("VarCheckFloat", -0.03125),
            ("VarCheckDouble", -0.004),
            ("VarCheckString", "ModifiedTestString"),
        ],
    )
    def test_write_variable(self, test_inst, pvName, writeVal):
        """
        Write a known value to the opcua server via the
        output PV linked to the variable. Read back via
        the input PV and check the values match.
        Parametrised for all supported datatypes.
        """
        ioc = test_inst.IOC

        with ioc:
            # Output PV name is the same as the input PV
            # name, with the addition of the "Out" suffix
            pvOutName = pvName + "Out"
            pvWrite = PV(pvOutName)
            assert ioc.is_running()
            assert (
                pvWrite.put(writeVal, wait=True, timeout=test_inst.putTimeout)
                is not None
            ), ("Failed to write to PV %s\n" % pvOutName)
            # Read back via input PV
            pvRead = PV(pvName)
            assert ioc.is_running()
            res = pvRead.get(use_monitor=False, timeout=test_inst.getTimeout)
            retryCnt = 0
            while res is None:
                print("%d: Read timeout. Try again...\n" % retryCnt)
                ioc.exit()
                ioc.start()
                res = pvRead.get(
                    use_monitor=False, timeout=test_inst.getTimeout
                )  # NoQA: E501
                if retryCnt > 3:
                    break

            # Compare
            assert res == writeVal
