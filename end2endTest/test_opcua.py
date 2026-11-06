import os
import shutil
import subprocess
import sys
import threading
import time
import re
from collections.abc import Generator
from pathlib import Path
from time import sleep, monotonic

# The test IOC runs on this host: keep Channel Access local (must be set before
# pyepics creates its CA context; broadcasts to localhost are unreliable on Windows)
os.environ.setdefault("EPICS_CA_AUTO_ADDR_LIST", "NO")
os.environ.setdefault("EPICS_CA_ADDR_LIST", "127.0.0.1")

import epics
import pytest
import run_iocsh.ioc
from opcua_test_server import OpcuaTestServer
from epics import caput, PV
from run_iocsh import IOC

if sys.platform == "win32":
    # run-iocsh stops a hanging IOC with os.killpg()/SIGKILL, which do not exist
    # on Windows. The test IOC spawns no children, so killing the process is enough.
    def _terminate_process(proc: subprocess.Popen, pgid: int) -> bool:
        if proc.poll() is not None:
            return False
        proc.kill()
        proc.wait()
        return True

    run_iocsh.ioc._terminate_group = _terminate_process


def caget(pv_name, **kwargs):
    """caget that always reads from the IOC.

    pyepics caches PVs (with a monitor) across tests, i.e. across IOC instances,
    so a plain caget() may return a stale value from a previous test's IOC.
    """
    kwargs.setdefault("use_monitor", False)
    return epics.caget(pv_name, **kwargs)


def dump_threads(pid: int) -> None:
    """Print backtraces of all threads of a (hanging) process using gdb."""
    gdb = shutil.which("gdb")
    if gdb is None:
        print(f"=== cannot dump threads of IOC (pid {pid}): gdb not found")
        return
    cmd = [gdb, "-p", str(pid), "-batch", "-nx",
           "-ex", "set pagination off", "-ex", "thread apply all bt"]
    # Attaching to a non-child process needs privileges (ptrace_scope)
    attempts = [cmd]
    if hasattr(os, "geteuid") and os.geteuid() != 0 and shutil.which("sudo"):
        attempts.insert(0, ["sudo", "-n", *cmd])
    res = None
    for attempt in attempts:
        try:
            res = subprocess.run(attempt, capture_output=True, text=True, timeout=30)
        except (OSError, subprocess.TimeoutExpired) as exc:
            print(f"=== thread dump with '{attempt[0]}' failed: {exc}")
            continue
        if "Thread" in res.stdout:
            print(f"=== IOC (pid {pid}) is hanging, backtraces of all threads:")
            print(res.stdout)
            return
    print(f"=== cannot dump threads of IOC (pid {pid}):\n{res.stderr if res else ''}")


class DiagIOC(IOC):
    """IOC that dumps its threads if it does not exit in time."""

    def exit(self) -> None:
        watchdog = None
        if self.is_running() and self.exit_timeout:
            # Dump before run-iocsh kills the IOC at exit_timeout
            watchdog = threading.Timer(max(self.exit_timeout - 4.0, 1.0),
                                       lambda pid=self.proc.pid:
                                       self.is_running() and dump_threads(pid))
            watchdog.start()
        try:
            super().exit()
        finally:
            if watchdog is not None:
                watchdog.cancel()
                watchdog.join()


@pytest.fixture
def test_inst(monkeypatch) -> Generator[tuple[OpcuaTestServer, IOC]]:
    script = Path(__file__).parent / "ioc" / "st.cmd"
    script = script.resolve()
    REPO_ROOT = Path(__file__).resolve().parents[1]
    host_arch = os.environ["EPICS_HOST_ARCH"]
    exe_suffix = ".exe" if sys.platform == "win32" else ""
    OPCUA_TEST_IOC = REPO_ROOT / "bin" / host_arch / f"opcuaTestIoc{exe_suffix}"
    # iocsh treats backslashes as escapes: use forward slashes (fine on Windows)
    monkeypatch.setenv("OPCUA_TEST_TOP", REPO_ROOT.as_posix())
    # Comment out loading the 64-bit array records on Base < 3.16.1
    monkeypatch.setenv("NO_INT64", "" if base_supports_int64() else "#")
    # Session debug output (only shown for failing tests)
    monkeypatch.setenv("OPCUA_DEBUG", os.environ.get("OPCUA_DEBUG", "1"))

    ioc = DiagIOC(str(script), executable=str(OPCUA_TEST_IOC))
    try:
        with OpcuaTestServer() as server, ioc:
            ioc.wait_for_output("OPC UA session")
            # LastPVLiveCheck only gets its value through the subscription (bini=ignore),
            # so this also waits for the monitored items to be established
            assert wait_for_value("LastPVLiveCheck", 42, timeout=20.0), \
                "IOC not ready: no subscription update for LastPVLiveCheck"
            yield server, ioc
    finally:
        # pytest only shows this for failing tests
        print("=== IOC output:")
        print(ioc.output)

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
        value = caget(pv_name)
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

def base_supports_int64() -> bool:
    version_h = Path(os.environ["EPICS_BASE"]) / "include" / "epicsVersion.h"
    text = version_h.read_text()

    major = int(re.search(r"#define EPICS_VERSION\s+(\d+)", text).group(1))
    revision = int(re.search(r"#define EPICS_REVISION\s+(\d+)", text).group(1))
    modification = int(
        re.search(r"#define EPICS_MODIFICATION\s+(\d+)", text).group(1)
    )

    return (major, revision, modification) >= (3, 16, 1)

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
        assert ioc.is_running()

        # Test using alarm severity.
        # Connection loss detection time depends on the client library
        # (UA SDK only reports it after its watchdog times out), so poll.
        assert wait_for_value("VarCheckBool.SEVR", 3, timeout=20.0)  # INVALID
        assert ioc.is_running()

        server.reconnect_to_clients()

        # Allow for reconnect and initial record processing
        assert wait_for_value("VarCheckBool.SEVR", 0, timeout=20.0)  # NO_ALARM

        # The initial read finishes before the monitored items are re-created.
        # Wait for subscription updates (TstRamp changes every second on the server;
        # the first change might still come from the initial read) so that the
        # session is fully re-established before exiting.
        prev = caget("TstRamp")
        for _ in range(2):
            now = wait_for_change("TstRamp", prev, timeout=10.0)
            assert now != prev, "no subscription updates after reconnect"
            prev = now

        ioc.exit()
        assert not ioc.is_running()
        assert server.exception is None

    def test_exit_during_reconnect(self, test_inst) -> None:
        server, ioc = test_inst

        server.disconnect_from_clients()
        assert wait_for_value("VarCheckBool.SEVR", 3, timeout=20.0)  # INVALID

        server.reconnect_to_clients()

        # Exit as soon as the initial read is done, i.e. while the session
        # is (most likely) still re-creating its monitored items.
        # The IOC must shut down cleanly (no hang, no crash).
        assert wait_for_value("VarCheckBool.SEVR", 0, timeout=20.0)  # NO_ALARM
        ioc.exit()
        assert not ioc.is_running()
        assert server.exception is None


class TestVariable:
    def test_variable_pvramp(self, test_inst) -> None:
        # Variable on the OPCUA server increments by 1 each second
        pv_name = "TstRamp"
        capture_len = 5

        prev = caget(pv_name)
        for _ in range(capture_len):
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

    @pytest.mark.skipif(not base_supports_int64(), reason="64-bit waveform types require EPICS Base >= 3.16.1")
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
        server, _ = test_inst
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
