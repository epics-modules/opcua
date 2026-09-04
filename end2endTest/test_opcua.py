import os
import resource
import time
from collections.abc import Generator
from pathlib import Path
from time import sleep

import pytest
from opcua_test_server import OpcuaTestServer
from p4p.client.thread import Context
from run_iocsh import IOC


@pytest.fixture
def test_inst() -> Generator[tuple[OpcuaTestServer, IOC, Context]]:
    script = Path(__file__).parent / "ioc" / "st.cmd"
    script = script.resolve()
    REPO_ROOT = Path(__file__).resolve().parents[1]

    OPCUA_TEST_IOC = (
        REPO_ROOT
        / "bin"
        / "linux-x86_64"
        / "opcuaTestIoc"
    )
    
    with OpcuaTestServer() as server, IOC(str(script), executable=str(OPCUA_TEST_IOC)) as ioc, Context("pva") as ctxt:
        ioc.wait_for_output("OPC UA session")
        sleep(5)  # Allow for initial record processing
        yield server, ioc, ctxt


class TestConnection:
    def test_connect_disconnect(self, test_inst) -> None:
        server, ioc, _ = test_inst
        assert ioc.is_running()
        ioc.exit()
        assert not ioc.is_running()
        assert server.exception is None

    def test_server_reconnect(self, test_inst) -> None:
        server, ioc, ctxt = test_inst

        server.disconnect_from_clients()
        sleep(1)
        assert ioc.is_running()

        # Test using alarm severity
        pv = ctxt.get("VarCheckBool")
        assert pv.severity == 3  # INVALID

        server.reconnect_to_clients()
        sleep(5)  # Allow for initial record processing

        pv = ctxt.get("VarCheckBool")
        assert pv.severity == 0  # NO_ALARM

        ioc.exit()
        assert not ioc.is_running()
        assert server.exception is None


class TestVariable:
    def test_variable_pvramp(self, test_inst) -> None:
        # Variable on the OPCUA server increments by 1 each second
        _, _, ctxt = test_inst
        pv_name = "TstRamp"
        capture_len = 5
        capture_incr = 5

        prev = ctxt.get(pv_name)
        for i in range(capture_len):
            sleep(capture_incr)
            now = ctxt.get(pv_name)
            assert now - prev == pytest.approx(5, abs=1)
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
        _, _, ctxt = test_inst
        res = ctxt.get(pv_name)
        # Check 64 bit integers with correct scientific notation
        if pv_name == "VarCheckUInt64" or pv_name == "VarCheckInt64":
            res = "%.16e" % res
        # Compare
        assert res == expected_val

    def test_read_array(self, test_inst) -> None:
        _, _, ctxt = test_inst
        res = ctxt.get("VarCheckUInt64Array")
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
        _, _, ctxt = test_inst
        pv_out_name = pv_name + "Out"
        assert ctxt.put(pv_out_name, write_val) is None, (
            "Failed to write to PV %s\n" % pv_out_name
        )
        sleep(1)
        assert ctxt.get(pv_name) == write_val

    def test_timestamps(self, test_inst) -> None:
        server, _, ctxt = test_inst
        time_var = ctxt.get("VarCheckStaticTimeStamp")
        expected_ts = server.fixed_time.timestamp()
        assert time_var.timestamp == pytest.approx(expected_ts, abs=1)

    def test_monitor(self, test_inst) -> None:
        server, _, ctxt = test_inst
        server.write_server_value(f"ns={server.idx};s=Sim.VarCheckInt16NoMonitor", 17)
        server.write_server_value(f"ns={server.idx};s=Sim.VarCheckInt16Monitor", 18)
        sleep(1)
        assert ctxt.get("VarCheckInt16OutNoMonitor") == -5
        assert ctxt.get("VarCheckInt16OutMonitor") == 18

    def test_bini(self, test_inst) -> None:
        server, ioc, ctxt = test_inst
        assert ctxt.get("VarCheckInt16NoBini") == 0
        assert ctxt.get("VarCheckInt16WriteBini") == 7
        
class TestPerformance:
    @pytest.mark.skipif(os.environ.get("CI") == "true", reason="Skipped in CI")
    def test_write_performance(self, test_inst) -> None:
        _, _, ctxt = test_inst

        writes = 100

        # Get time and memory conspumtion before test
        mem_start = resource.getrusage(resource.RUSAGE_THREAD).ru_maxrss
        time_start = time.perf_counter()

        # Write 100 PVs
        for i in range(writes):
            ctxt.put("VarCheckInt16Out", i)

        # Get time and memory consumption during test
        mem_delta = resource.getrusage(resource.RUSAGE_THREAD).ru_maxrss - mem_start
        time_delta = time.perf_counter() - time_start

        # Should be able to read in less than 10 ms on most systems
        assert time_delta < (writes * 0.001)

        # Memory consumption should be minimal
        assert mem_delta < 100

    @pytest.mark.skipif(os.environ.get("CI") == "true", reason="Skipped in CI")
    def test_read_performance(self, test_inst) -> None:
        _, _, ctxt = test_inst

        reads = 100

        # Get time and memory conspumtion before test
        mem_start = resource.getrusage(resource.RUSAGE_THREAD).ru_maxrss
        time_start = time.perf_counter()

        # Read 100 PVs
        for i in range(reads):
            ctxt.get("VarCheckInt16")

        # Get time and memory consumption during test
        mem_delta = resource.getrusage(resource.RUSAGE_THREAD).ru_maxrss - mem_start
        time_delta = time.perf_counter() - time_start

        # Should be able to read a node in less than 1 ms on most systems
        assert time_delta < (reads * 0.001)

        # Memory consumption should be minimal
        assert mem_delta < 100


class TestNegative:
    def test_bad_var_name(self, test_inst) -> None:
        _, _, ctxt = test_inst
        val = ctxt.get("BadVarName")
        assert val.severity == 3

    def test_wrong_datatype(self, test_inst) -> None:
        _, _, ctxt = test_inst
        val = ctxt.get("VarNotBoolean")
        assert val.severity == 3

    def test_write_non_writable(self, test_inst) -> None:
        server, _, ctxt = test_inst
        assert (
            server.read_server_value(f"ns={server.idx};s=Sim.TestVarInt16NoWrite") == 32
        )

        ctxt.put("VarCheckInt16OutNoWrite", 221)
        sleep(1)

        assert ctxt.get("VarCheckInt16OutNoWrite", timeout=20) == 221
        assert (
            server.read_server_value(f"ns={server.idx};s=Sim.TestVarInt16NoWrite") == 32
        )
