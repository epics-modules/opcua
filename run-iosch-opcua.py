from epics import PV
from time import sleep
from run_iocsh import IOC

opcuaTestArgs = ["-l", "cellMods", "-r", "opcua,0.8.0-test"]
cmd = "test/cmds/test_pv.cmd"


def test_connect_disconnect():
    """
    Connect and disconnect to the OPC-UA test server, and
    check that there are no errors
    """
    ioc = IOC(*opcuaTestArgs, cmd)
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
        connectMsg = (
            "OPC UA session OPC1: connection status changed"
            + " from Connected to Disconnected"
        )
        assert (
            output.find(connectMsg) >= 0
        ), "%d: Failed to find disconnect message in output\n%s" % (i, output)
        print(output)


def test_variable_pvget():
    """
    Variable on the OPCUA server increments by 1 each second
    Sample the value using pvget once a second and check the
    value is incrmenting.
    """
    with IOC(*opcuaTestArgs, cmd):
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
