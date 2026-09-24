import os
import resource
import time
from collections.abc import Generator
from pathlib import Path
from time import sleep, monotonic

import pytest
from opcua_test_server import OpcuaTestServer
from epics import caget, caput, PV
from run_iocsh import IOC


@pytest.fixture
def test_inst() -> Generator[tuple[OpcuaTestServer, IOC]]:
    script = Path(__file__).parent / "ioc" / "st.cmd"
    script = script.resolve()
    REPO_ROOT = Path(__file__).resolve().parents[1]
    host_arch = os.environ["EPICS_HOST_ARCH"]
    OPCUA_TEST_IOC = REPO_ROOT / "bin" / host_arch / "opcuaTestIoc"
    
    with OpcuaTestServer() as server, IOC(str(script), executable=str(OPCUA_TEST_IOC)) as ioc:
        ioc.wait_for_output("OPC UA session")
        sleep(5)  # Allow for initial record processing
        yield server, ioc

def wait_for_change(pv_name, previous, timeout=2.0):
    deadline = monotonic() + timeout
    while monotonic() < deadline:
        current = caget(pv_name)
        if current != previous:
            return current
        sleep(0.05)
    return previous

def wait_for_value(pv_name, expected, timeout=2.0):
    deadline = monotonic() + timeout
    while monotonic() < deadline:
        value = caget(pv_name, use_monitor=False)
        if value == expected:
            return True
        sleep(0.05)
    return False

def wait_for_server_value(server, nodeid, expected, timeout=1.0):
    deadline = monotonic() + timeout
    while monotonic() < deadline:
        if server.read_server_value(nodeid) == expected:
            return True
        sleep(0.05)
    return False

class TestConnection:
    def test_connect_disconnect(self, test_inst) -> None:
        server, ioc = test_inst
        assert ioc.is_running()
        ioc.exit()
        assert not ioc.is_running()
        assert server.exception is None

    def test_server_reconnect(self, test_inst) -> None:
        server, ioc = test_inst

        server.disconnect_from_clients()
        sleep(1)
        assert ioc.is_running()

        # Test using alarm severity
        pv = PV("VarCheckBool")
        pv.get()
        assert pv.severity == 3  # INVALID

        server.reconnect_to_clients()
        sleep(5)  # Allow for initial record processing

        pv = PV("VarCheckBool")
        pv.get()
        assert pv.severity == 0  # NO_ALARM

        ioc.exit()
        assert not ioc.is_running()
        assert server.exception is None


class TestVariable:
    def test_variable_pvramp(self, test_inst) -> None:
        # Variable on the OPCUA server increments by 1 each second
        pv_name = "TstRamp"
        capture_len = 5
        capture_incr = 5

        prev = caget(pv_name)
        for i in range(capture_len):
            now = wait_for_change(pv_name, prev)
            assert now - prev == 1
            prev = now

    @pytest.mark.parametrize(
        "pv_name,expected_val",
        [
            ("VarCheckBool", True),
            ("VarCheckSByte", -128),
            ("VarCheckByte", 255),
            ("VarCheckInt16", -32768),
            ("VarCheckUInt16", 65535),
            ("VarCheckInt32", -2147483648),
            ("VarCheckUInt32", 4294967295),
            ("VarCheckInt64", f"{-9223372036854775805:.16e}"),
            ("VarCheckUInt64", f"{9223372036854775809:.16e}"),
            ("VarCheckFloat", -0.0625),
            ("VarCheckDouble", 1.0000000000000002),
            ("VarCheckString", "TestString01"),
            ("VarCheckStructNodeNamespaceUri", "http://epics-controls.org/OpcUa"),
            ("VarCheckStructNodeUnitId", 42),
        ],
    )
    def test_read_variable(self, test_inst, pv_name, expected_val) -> None:
        res = caget(pv_name)
        # Check 64 bit integers with correct scientific notation
        if pv_name == "VarCheckUInt64" or pv_name == "VarCheckInt64":
            res = "%.16e" % res
        # Compare
        assert res == expected_val

    def test_read_array(self, test_inst) -> None:
        res = caget("VarCheckUInt64Array")
        assert res[0] == 3
        assert res[1] == 9
        assert res[2] == 12

    @pytest.mark.parametrize(
        "pv_name,write_val",
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
    def test_write_variable(self, test_inst, pv_name, write_val) -> None:
        pv_out_name = pv_name + "Out"
        assert caput(pv_out_name, write_val)
        assert wait_for_value(pv_name, write_val)

    def test_timestamps(self, test_inst) -> None:
        server, _ = test_inst
        pv = PV("VarCheckStaticTimeStamp")
        pv.get()
        expected_ts = server.fixed_time.timestamp()
        assert pv.timestamp == pytest.approx(expected_ts, abs=1)

    def test_monitor(self, test_inst) -> None:
        server, _ = test_inst
        server.write_server_value(f"ns={server.idx};s=Sim.VarCheckInt16NoMonitor", 17)
        server.write_server_value(f"ns={server.idx};s=Sim.VarCheckInt16Monitor", 18)
        assert wait_for_value("VarCheckInt16OutMonitor", 18)
        assert caget("VarCheckInt16OutNoMonitor") == -5

    def test_bini(self, test_inst) -> None:
        server, ioc = test_inst
        assert caget("VarCheckInt16NoBini") == 0
        assert server.read_server_value(f"ns={server.idx};s=Sim.VarCheckInt16NoBini") == 112
        assert server.read_server_value(f"ns={server.idx};s=Sim.VarCheckInt16WriteBini") == 7

class TestNegative:
    def test_bad_var_name(self, test_inst) -> None:
        pv = PV("BadVarName")
        pv.get()
        assert pv.severity == 3

    def test_wrong_datatype(self, test_inst) -> None:
        pv = PV("VarNotBoolean")
        pv.get()
        assert pv.severity == 3

    def test_write_non_writable(self, test_inst) -> None:
        server, _= test_inst
        assert (server.read_server_value(f"ns={server.idx};s=Sim.TestVarInt16NoWrite") == 32)
        caput("VarCheckInt16OutNoWrite", 221)
        assert not wait_for_server_value(server, f"ns={server.idx};s=Sim.TestVarInt16NoWrite", 221)
        assert caget("VarCheckInt16OutNoWrite", timeout=20) == 221
