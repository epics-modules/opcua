import gc
import os
import resource
import signal
import subprocess
import time
from datetime import datetime
from os import environ
from time import sleep

import pytest
from epics import PV, ca
from run_iocsh import IOC


class opcuaTestHarness:
    def __init__(self):

        # Get values from the environment
        self.EPICS_BASE = environ.get("EPICS_BASE")
        self.REQUIRE_VERSION = environ.get("E3_REQUIRE_VERSION")

        # Run CA on localhost
        environ["EPICS_CA_ADDR_LIST"] = "127.0.0.1"
        environ["EPICS_CA_AUTO_ADDR_LIST"] = "NO"

        if self.REQUIRE_VERSION is None:
            # Run as part of the opcua Device Support
            # tests are in 'end2endTest'
            self.TESTSUBDIR = "end2endTest"
            self.EPICS_HOST_ARCH = environ.get("EPICS_HOST_ARCH")
            self.IOC_TOP = environ.get("IOC_TOP")
            if self.IOC_TOP is None:
                self.IOC_TOP = os.getcwd() + "/exampleTop"
                environ["IOC_TOP"] = self.IOC_TOP
            # run-iocsh parameters
            self.IOCSH_PATH = (
                f"{self.IOC_TOP}/bin/{self.EPICS_HOST_ARCH}/opcuaIoc"
            )
            self.TestArgs = []
            # Set path to opcua.iocsh snippet
            environ["opcua_DIR"] = f"{self.TESTSUBDIR}/cmds"

            environ["EPICS_DB_INCLUDE_PATH"] = f"{self.TESTSUBDIR}/db:{self.IOC_TOP}/db"
            environ["LD_LIBRARY_PATH"] = self.EPICS_BASE + "/lib/" + self.EPICS_HOST_ARCH

        else:
            # Run under E3
            # tests are in 'test'
            self.TESTSUBDIR = "test"
            self.MOD_VERSION = environ.get("E3_MODULE_VERSION")
            if self.MOD_VERSION is None:
                self.MOD_VERSION = "0.8.0"
            self.TEMP_CELL_PATH = environ.get("TEMP_CELL_PATH")
            if self.TEMP_CELL_PATH is None:
                self.TEMP_CELL_PATH = "cellMods"

            # run-iocsh parameters
            self.IOCSH_PATH = f"{self.EPICS_BASE}/require/{self.REQUIRE_VERSION}/bin/iocsh"

            self.TestArgs = [
                "-l",
                self.TEMP_CELL_PATH,
                "-r",
                f"opcua,{self.MOD_VERSION}",
            ]

            environ["EPICS_DB_INCLUDE_PATH"] = f"{self.TESTSUBDIR}/db"

        self.cmd = f"{self.TESTSUBDIR}/cmds/test_pv.cmd"
        self.neg_cmd = f"{self.TESTSUBDIR}/cmds/test_pv_neg.cmd"
        self.testServer = f"{self.TESTSUBDIR}/server/opcuaTestServer"

        # Default IOC
        self.IOC = self.get_ioc()

        # timeout value in seconds for pvput/pvget calls
        self.timeout = 5
        self.putTimeout = self.timeout
        self.getTimeout = self.timeout

        # test sleep time ins seconds
        self.sleepTime = 3

        # Test server
        self.isServerRunning = False
        self.serverURI = "opc.tcp://127.0.0.1:4840"
        self.serverFakeTime = "2019-05-02 09:22:52"

        # Message catalog
        self.connectMsg = (
            "OPC UA session OPC1: connected as 'Anonymous'"
        )
        self.disconnectMsg = (
            "OPC UA session OPC1: disconnected"
        )

        self.badNodeIdMsg = "item ns=2;s=Sim.BadVarName : BadNodeIdUnknown"

        # Server variables
        self.serverVars = [
            "open62541",
            "open62541 OPC UA Server",
            "1.2.0-29-g875d33a9",
        ]

    def get_ioc(self, cmd=None):
        if cmd is None:
            cmd = self.cmd
        return IOC(
            *self.TestArgs,
            cmd,
            ioc_executable=self.IOCSH_PATH,
        )

    def start_server(self, withPIPE=False):
        if withPIPE:
            self.serverProc = subprocess.Popen(
                self.testServer,
                shell=False,
                stdout=subprocess.PIPE,
            )
        else:
            self.serverProc = subprocess.Popen(
                self.testServer,
                shell=False,
            )

        print("\nOpening server with pid = %s" % self.serverProc.pid)
        retryCount = 0
        while (not self.isServerRunning) and retryCount < 5:
            # Poll server to see if it is running
            self.is_server_running()
            retryCount = retryCount + 1
            sleep(1)

        assert retryCount < 5, "Unable to start server"

    def start_server_with_faketime(self):
        self.serverProc = subprocess.Popen(
            "faketime -f '%s' %s" % (self.serverFakeTime, self.testServer),
            shell=True,
            preexec_fn=os.setsid,
        )

        print("\nOpening server with pid = %s" % self.serverProc.pid)
        retryCount = 0
        while (not self.isServerRunning) and retryCount < 5:
            # Poll server to see if it is running
            self.is_server_running()
            retryCount = retryCount + 1
            sleep(1)

        assert retryCount < 5, "Unable to start server"

    def stop_server_group(self):
        # Get the process group ID for the spawned shell,
        # and send terminate signal
        print("\nClosing server group with pgid = %s" % self.serverProc.pid)
        os.killpg(os.getpgid(self.serverProc.pid), signal.SIGTERM)
        # Update if server is running
        self.is_server_running()

    def stop_server(self):
        print("\nClosing server with pid = %s" % self.serverProc.pid)
        # Send terminate signal
        self.serverProc.terminate()
        # Wait for processes to terminate.
        self.serverProc.wait(timeout=5)
        # Update if server is running
        self.is_server_running()

    def is_server_running(self):
        from opcua import Client

        c = Client(self.serverURI)
        try:
            # Connect to server
            c.connect()
            # NS0|2259 is the server state variable
            # 0 -- Running
            var = c.get_node("ns=0;i=2259")
            val = var.get_data_value()
            self.isServerRunning = val.StatusCode.is_good()
            # Disconnect from server
            c.disconnect()

        except Exception:
            self.isServerRunning = False


# Standard test fixture
@pytest.fixture(scope="function")
def test_inst():
    """
    Instantiate test harness, start the server,
    yield the harness handle to the test,
    close the server on test end / failure
    """

    # Run the garbage collector to work around bug in pyepics
    gc.collect()
    # Initialize channel access
    ca.initialize_libca()

    # Create handle to Test Harness
    test_inst = opcuaTestHarness()
    # Poll to see if the server is running
    test_inst.is_server_running()
    assert not (
        test_inst.isServerRunning
    ), "An instance of the OPC-UA test server is already running"
    # Start server
    test_inst.start_server()
    # Drop to test
    yield test_inst

    # Shutdown server by sending terminate signal
    test_inst.stop_server()
    # Check server is stopped
    assert not test_inst.isServerRunning

    # Shut down channel access
    ca.flush_io()
    ca.clear_cache()
    ca.finalize_libca()


# test fixture for use with timezone server
@pytest.fixture(scope="function")
def test_inst_TZ():
    """
    Instantiate test harness, start the server,
    yield the harness handle to the test,
    close the server on test end / failure
    """
    # Run the garbage collector to work around bug in pyepics
    gc.collect()
    # Initialize channel access
    ca.initialize_libca()

    # Create handle to Test Harness
    test_inst_TZ = opcuaTestHarness()
    # Poll to see if the server is running
    test_inst_TZ.is_server_running()
    assert not (
        test_inst_TZ.isServerRunning
    ), "An instance of the OPC-UA test server is already running"
    # Start server
    test_inst_TZ.start_server_with_faketime()
    # Drop to test
    yield test_inst_TZ

    # Shutdown server by sending terminate signal
    test_inst_TZ.stop_server_group()
    # Check server is stopped
    assert not test_inst_TZ.isServerRunning

    # Shut down channel access
    ca.flush_io()
    ca.clear_cache()
    ca.finalize_libca()

def wait_for_value(pv, expValue, timeout=0, format=None):
    """
    Wait for a (monitored) PV to reach the expected value,
    up to a specified timeout. (returns None on timeout.)
    """
    t0 = time.perf_counter()
    dt = 0.0

    while dt < timeout:
        res = pv.get()
        if format is not None:
            res = format % res
        if res == expValue:
            return res
        sleep(0.1)
        dt = time.perf_counter() - t0

    return None

class TestConnectionTests:
    nRuns = 5

    def test_start_server_then_ioc(self, test_inst):
        """
        Start the server and an IOC.
        Check that the IOC connects.
        """

        ioc = test_inst.IOC

        for i in range(0, self.nRuns):
            # Start IOC, and check it is running
            ioc.start()
            assert ioc.is_running()

            ioc.exit()
            assert not ioc.is_running()

            # Grab ioc error output
            ioc.check_output()
            output = ioc.errs

            # Parse for OPC-UA connection message
            assert (
                output.find(test_inst.connectMsg) >= 0
            ), "%d: Failed to find connect message\n%s" % (i, output)

            print(output)

    def test_stop_and_restart_server(self, test_inst):
        """
        Start the server and an IOC.
        Stop the server, check for appropriate messaging.
        Start the server, check that the IOC reconnects.
        """
        ioc = test_inst.IOC

        for i in range(0, self.nRuns):
            ioc.start()
            assert ioc.is_running()

            sleep(test_inst.sleepTime)

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
            output = ioc.errs
            print(output)

            # Check for OPC UA dis/connection messages
            assert (
                output.find(test_inst.disconnectMsg) >= 0
            ), "%d: Failed to find disconnect message\n%s" % (i, output)
            connect1Pos = output.find(test_inst.connectMsg)
            assert (
                connect1Pos >= 0
            ), "%d: Failed to find 1st connect message\n%s" % (i, output)
            assert (
                output.find(test_inst.connectMsg, connect1Pos + 1) >= 0
            ), "%d: Failed to find 2nd connect message\n%s" % (i, output)

    def test_start_ioc_then_server(self, test_inst):
        """
        Start an IOC with no server running.
        Start the server, check that the IOC connects.
        """

        ioc = test_inst.IOC

        for i in range(0, self.nRuns):
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
            output = ioc.errs
            print(output)

            # Parse for OPC-UA connection message
    # As of opcua 0.9, the IOC doesn't report failed connection on startup
    #        assert (
    #            output.find(test_inst.noConnectMsg) >= 0
    #        ), "%d: Failed to find no connection message\n%s" % (i, output)
            assert (
                output.find(test_inst.connectMsg) >= 0
            ), "%d: Failed to find connect message in output\n%s" % (i, output)

    def test_disconnect_on_ioc_exit(self, test_inst):
        """
        Start the server and an IOC. Ensure successful connection.
        Exit the IOC and ensure that session and subscription
        are properly disconnected.
        """

        ioc = test_inst.IOC

        for i in range(0, self.nRuns):

            # Start server with stdout PIPE (to fetch output)
            test_inst.stop_server()
            test_inst.start_server(withPIPE=True)

            ioc.start()
            assert ioc.is_running()

            # Wait a second to allow it to get up and running
            sleep(1)

            ioc.exit()
            assert not ioc.is_running()

            # Shutdown the server to allow getting the stdout messages
            test_inst.stop_server()

            # Read server's stdout buffer
            log = ""
            for line in iter(test_inst.serverProc.stdout.readline, b""):
                log = log + line.decode("utf-8")
            print(log)

            # Check the Session was activated
            assert log.find("ActivateSession: Session activated") >= 0, (
                "%d: Failed to find ActivateSession message: %s" % (i, log)
            )
            # Check Subscription was created
            assert log.find("Subscription 1 | Created the Subscription") >= 0, (
                "%d: Failed to find Subscription message: %s" % (i, log)
            )

            # Find the position in the log where the terminate signal
            # is received
            termPos = log.find("received ctrl-c")

            # Check that the session and subscription close messages
            # occur before the terminate signal is received.
            # This means they were closed when the IOC was shutdown
            closePos = log.find("Closing the Session")
            deletePos = log.find("Subscription 1 | Subscription deleted")
            assert 0 <= closePos <= termPos, (
                "%d: Session closed by terminate, not by IOC shutdown: %s" % (i, log)
            )
            assert 0 <= deletePos <= termPos, (
                "%d: Subscription closed by terminate, not by IOC shutdown: %s" % (i, log)
            )

            # Grab ioc output
            ioc.check_output()
            output = ioc.outs
            print(output)


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
            captureIncr = 5  # Time increment in seconds

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
                expected = resCheck[i - 1] + captureIncr
                print(
                    "Captured value (%d) is %d. Expected %d +/-1"
                    % (
                        i,
                        res[i],
                        expected,
                    )
                )
                assert (
                    expected - 1 <= resCheck[i] <= expected + 1
                ), "Captured value (%d) is %d. Expected %d +/-1" % (
                    i,
                    res[i],
                    expected,
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
            fmt = None
            if pvName == "VarCheckUInt64":
                fmt = "%.16e"
            res = wait_for_value(pv, expectedVal, timeout=test_inst.getTimeout, format=fmt)
            assert res == expectedVal
            pv.disconnect()

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
            assert ioc.is_running()
            # Output PV name is the same as the input PV
            # name, with the addition of the "Out" suffix
            pvOutName = pvName + "Out"
            pvWrite = PV(pvOutName)
            pvWrite.wait_for_connection()
            assert (
                pvWrite.put(writeVal, wait=True, timeout=test_inst.putTimeout)
                is not None
            ), ("Failed to write to PV %s\n" % pvOutName)

            # Read back via input PV
            assert ioc.is_running()
            pvRead = PV(pvName)
            res = wait_for_value(pvRead, writeVal, timeout=test_inst.getTimeout)
            assert res == writeVal
            pvWrite.disconnect()
            pvRead.disconnect()

    @pytest.mark.xfail
    def test_timestamps(self, test_inst_TZ):
        """
        Start the test server in a shell session with
        with a fake time in the past. Check that the
        timestamp for the PV read matches the known
        fake time given to the server.
        If they match, the OPCUA EPICS module is
        correctly pulling the timestamps from the
        OPCUA server (and not using a local TS)
        """

        ioc = test_inst_TZ.IOC

        # Get PV timestamp:

        with ioc:
            pvName = PV("TstRamp", form="time")
            pvName.get(timeout=test_inst_TZ.getTimeout)
            epicsTs = pvName.timestamp

        form = "%Y-%m-%d %H:%M:%S"
        pyTs = datetime.strptime(test_inst_TZ.serverFakeTime, form).timestamp()

        assert epicsTs == pyTs, "Timestamp returned does not match"


class TestPerformanceTests:
    @pytest.mark.xfail("CI" in environ, reason="CI runner performance varies")
    def test_write_performance(self, test_inst):
        """
        Write 5000 variable values and measure
        time and memory consumption before
        and after. Repeat 10 times
        """
        ioc = test_inst.IOC

        with ioc:
            # Get PV
            pvWrite = PV("VarCheckInt16Out")

            # Check that IOC is running
            assert ioc.is_running()

            maxt = 0
            mint = float("inf")
            tott = 0
            totr = 0
            testruns = 10
            writeperrun = 5000

            # Run test 10 times
            for j in range(1, testruns):

                # Get time and memory conspumtion before test
                r0 = resource.getrusage(resource.RUSAGE_THREAD)
                t0 = time.perf_counter()

                # Write 5000 PVs
                for i in range(1, writeperrun):
                    pvWrite.put(i, wait=True, timeout=test_inst.putTimeout)

                # Get delta time and delta memory
                dt = time.perf_counter() - t0
                r1 = resource.getrusage(resource.RUSAGE_THREAD)
                dr = r1.ru_maxrss - r0.ru_maxrss  # NoQA: E501

                # Collect data for statistics
                if dt > maxt:
                    maxt = dt
                if dt < mint:
                    mint = dt
                tott += dt
                totr += dr
                print("Time: ", "{:.3f} s".format(dt))
                print("Memory incr: ", dr)
                print("     before: ", r0.ru_maxrss)
                print("      after: ", r1.ru_maxrss)

            avgt = tott / testruns

            print("Max time: ", "{:.3f} s".format(maxt))
            print("Min time: ", "{:.3f} s".format(mint))
            print("Average time: ", "{:.3f} s".format(avgt))
            print("Total memory increase: ", totr)

            assert maxt < 17
            assert mint > 0.8
            assert avgt < 5
            assert totr < 3000

    @pytest.mark.xfail("CI" in environ, reason="CI runner performance varies")
    def test_read_performance(self, test_inst):
        """
        Read 5000 variable values and measure time and
        memory consumption before and after.
        Repeat 10 times
        """
        ioc = test_inst.IOC

        with ioc:
            # Get PV
            pvRead = PV("VarCheckInt16")

            # Check that IOC is running
            assert ioc.is_running()

            maxt = 0
            mint = float("inf")
            tott = 0
            totr = 0
            testruns = 10
            writeperrun = 5000

            # Run test 10 times
            for j in range(1, testruns):

                # Get time and memory conspumtion before test
                r0 = resource.getrusage(resource.RUSAGE_SELF)
                t0 = time.perf_counter()

                # Read 5000 PVs
                for i in range(1, writeperrun):
                    pvRead.get(timeout=test_inst.getTimeout)

                # Get delta time and delta memory
                dt = time.perf_counter() - t0
                r1 = resource.getrusage(resource.RUSAGE_SELF)
                dr = r1.ru_maxrss - r0.ru_maxrss  # NoQA: E501

                # Collect data for statistics
                if dt > maxt:
                    maxt = dt
                if dt < mint:
                    mint = dt
                tott += dt
                totr += dr
                print("Time: ", "{:.3f} s".format(dt))
                print("Memory incr: ", dr)
                print("     before: ", r0.ru_maxrss)
                print("      after: ", r1.ru_maxrss)

            avgt = tott / testruns

            print("Max time: ", "{:.3f} s".format(maxt))
            print("Min time: ", "{:.3f} s".format(mint))
            print("Average time: ", "{:.3f} s".format(avgt))
            print("Total memory increase: ", totr)

            assert maxt < 10
            assert mint > 0.01
            assert avgt < 7
            assert totr < 1000


class TestNegativeTests:
    def test_no_server(self, test_inst):
        """
        Start an OPC-UA IOC with no server running.
        Check the module reports this correctly.
        """

        ioc = test_inst.IOC

        # Stop the running server
        test_inst.stop_server()

        # Start the IOC
        ioc.start()
        assert ioc.is_running()

        # Check that PVs have SEVR INVALID (=3)
        pv = PV("VarCheckSByte")
        pv.get(timeout=test_inst.getTimeout)
        assert pv.severity == 3

        # Stop IOC, and check output
        ioc.exit()
        assert not ioc.is_running()

        ioc.check_output()
        output = ioc.outs
        print(output)

    def test_bad_var_name(self, test_inst):
        """
        Specify an incorrect variable name in a db record.
        Start the IOC and verify a sensible error is
        displayed.
        """

        # Use startup script for negative tests
        ioc = test_inst.get_ioc(cmd=test_inst.neg_cmd)

        # Start the IOC
        ioc.start()
        assert ioc.is_running()

        # Stop IOC, and check output
        ioc.exit()
        assert not ioc.is_running()

        ioc.check_output()
        output = ioc.errs
        print(output)

        assert output.find(test_inst.badNodeIdMsg) >= 0, (
            "Failed to find BadNodeIdUnknown message\n%s" % output
        )

    def test_wrong_datatype(self, test_inst):
        """
        Specify an incorrect record type for an OPC-UA variable.
        Binary input record for a float datatype.
        """
        import re

        # Use startup script for negative tests
        ioc = test_inst.get_ioc(cmd=test_inst.neg_cmd)

        # Start the IOC
        ioc.start()
        assert ioc.is_running()

        # Stop IOC, and check output
        ioc.exit()
        assert not ioc.is_running()

        ioc.check_output()
        output = ioc.errs
        print(output)

        regx = "VarNotBoolean : incoming data (.*) out-of-bounds"
        assert re.search(regx, output)
