import asyncio
import logging
import threading
from datetime import UTC, datetime
from types import TracebackType
from typing import Any, Self

from asyncua import Server, ua

NS_URI = "http://epics-controls.org/OpcUa"
RAMP_MAX_VALUE = 1000
RAMP_UPDATE_PERIOD_S = 1


class OpcuaTestServer(Server):
    def __init__( # noqa: D107
        self,
        endpoint: str = "opc.tcp://0.0.0.0:4840",
        server_name: str = "EPICS OPCUA asyncua test server",
    ) -> None: 
        self.fixed_time = datetime(2020, 1, 1, 12, 0, 0, tzinfo=UTC)

        self.loop = None
        self.thread = None
        self.ready = threading.Event()
        self.exception = None
        self.stop_requested = None
        self.idx = None

        super().__init__()
        self.set_endpoint(endpoint)
        self.set_server_name(server_name)

    def __enter__(self) -> Self: # noqa: D105
        self.ready.clear()
        self.exception = None

        self.thread = threading.Thread(
            target=self._thread_main,
            name="asyncua-test-server",
            daemon=True,
        )
        self.thread.start()

        if not self.ready.wait(timeout=20):
            raise TimeoutError("OPC UA test server did not start within 20 seconds")

        if self.exception is not None:
            exc = self.exception
            self.thread.join(timeout=1)
            raise RuntimeError("OPC UA test server failed to start") from exc

        if not self.thread.is_alive():
            raise RuntimeError("OPC UA test server exited during startup")

        return self


    def __exit__( # noqa: D105
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        thread = self.thread

        if self.loop is not None and self.stop_requested is not None:
            self.loop.call_soon_threadsafe(self.stop_requested.set)

        if thread is not None:
            thread.join(timeout=20)

            if thread.is_alive():
                raise RuntimeError("OPC UA test server did not stop within 20 seconds")

        self.thread = None
        self.loop = None
        self.stop_requested = None
        self.ready.clear()

    def running(self) -> bool:
        return (
            self.thread is not None
            and self.thread.is_alive()
            and self.exception is None
        )

    async def _disconnect_clients(self) -> None:
        await self.bserver.stop()

    async def _reconnect_clients(self) -> None:
        await self.bserver.start()

    def disconnect_from_clients(self) -> None:
        fut = asyncio.run_coroutine_threadsafe(
            self._disconnect_clients(),
            self.loop,
        )
        return fut.result(timeout=10)

    def reconnect_to_clients(self) -> None:
        fut = asyncio.run_coroutine_threadsafe(
            self._reconnect_clients(),
            self.loop,
        )
        return fut.result(timeout=10)

    async def _ramp_loop(self) -> None:
        ramp_node = self.get_node(f"ns={self.idx};s=Sim.TestRamp")
        count = 0

        while True:
            count = 0 if count >= RAMP_MAX_VALUE else count + 1

            await ramp_node.write_value(ua.Variant(float(count), ua.VariantType.Double))

            await asyncio.sleep(RAMP_UPDATE_PERIOD_S)

    def _thread_main(self) -> None:
        try:
            self.loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self.loop)
            self.loop.run_until_complete(self._run_server())
        except Exception as exc:  # noqa: BLE001 - propagate server thread failures to __enter__
            self.exception = exc
            self.ready.set()
        finally:
            if self.loop is not None:
                self.loop.close()

    def read_server_value(self, nodeid: str) -> Any:
        async def _read():
            return await self.get_node(nodeid).read_value()

        fut = asyncio.run_coroutine_threadsafe(_read(), self.loop)
        return fut.result(timeout=10)

    def write_server_value(self, nodeid: str, value: Any) -> None:
        async def _write():
            node = self.get_node(nodeid)

            current = await node.read_data_value()
            variant_type = current.Value.VariantType

            await node.write_value(ua.Variant(value, variant_type))

        fut = asyncio.run_coroutine_threadsafe(_write(), self.loop)
        return fut.result(timeout=10)

    async def _run_server(self) -> None:
        logging.basicConfig(level=logging.INFO)
        self.stop_requested = asyncio.Event()
        await self.init()

        self.set_force_server_timestamp(False)
        self.idx = await self.register_namespace(NS_URI)
        sim = await self.nodes.objects.add_object(self.idx, "Sim")

        eu = ua.EUInformation()
        eu.NamespaceUri = "http://epics-controls.org/OpcUa"
        eu.UnitId = 42
        eu.DisplayName = ua.LocalizedText("Test struct variable")
        eu.Description = ua.LocalizedText("Test struct description")

        variable_nodes = [
            ("Sim.TestRamp", sim, ua.Variant(0.0, ua.VariantType.Double), False, False),
            (
                "Sim.TestVarBool",
                sim,
                ua.Variant(True, ua.VariantType.Boolean),
                True,
                False,
            ),
            ("Sim.TestVarByte", sim, ua.Variant(255, ua.VariantType.Byte), True, False),
            (
                "Sim.TestVarInt16",
                sim,
                ua.Variant(-32768, ua.VariantType.Int16),
                True,
                False,
            ),
            (
                "Sim.TestVarUInt16",
                sim,
                ua.Variant(65535, ua.VariantType.UInt16),
                True,
                False,
            ),
            (
                "Sim.TestVarSByte",
                sim,
                ua.Variant(-128, ua.VariantType.SByte),
                True,
                False,
            ),
            (
                "Sim.TestVarDouble",
                sim,
                ua.Variant(1.0000000000000002, ua.VariantType.Double),
                True,
                False,
            ),
            (
                "Sim.TestVarFloat",
                sim,
                ua.Variant(-0.0625, ua.VariantType.Float),
                True,
                False,
            ),
            (
                "Sim.TestVarInt32",
                sim,
                ua.Variant(-2147483648, ua.VariantType.Int32),
                True,
                False,
            ),
            (
                "Sim.TestVarInt64",
                sim,
                ua.Variant(-9223372036854775805, ua.VariantType.Int64),
                True,
                False,
            ),
            (
                "Sim.TestVarString",
                sim,
                ua.Variant("TestString01", ua.VariantType.String),
                True,
                False,
            ),
            (
                "Sim.TestVarUInt32",
                sim,
                ua.Variant(4294967295, ua.VariantType.UInt32),
                True,
                False,
            ),
            (
                "Sim.TestVarUInt64",
                sim,
                ua.Variant(9223372036854775809, ua.VariantType.UInt64),
                True,
                False,
            ),
            (
                "Sim.VarCheckInt16NoMonitor",
                sim,
                ua.Variant(-5, ua.VariantType.Int16),
                False,
                False,
            ),
            (
                "Sim.VarCheckInt16Monitor",
                sim,
                ua.Variant(-11, ua.VariantType.Int16),
                False,
                False,
            ),
            (
                "Sim.VarCheckInt16NoBini",
                sim,
                ua.Variant(112, ua.VariantType.Int16),
                False,
                False,
            ),
            (
                "Sim.VarCheckInt16WriteBini",
                sim,
                ua.Variant(-71, ua.VariantType.Int16),
                True,
                False,
            ),
            (
                "Sim.TestVarInt16NoWrite",
                sim,
                ua.Variant(32, ua.VariantType.Int16),
                False,
                False,
            ),
            (
                "Sim.TestVarUInt64Array",
                sim,
                ua.Variant([3, 9, 12], ua.VariantType.UInt64),
                False,
                False,
            ),
            (
                "Sim.TestStaticTimestamp",
                sim,
                ua.Variant(42.0, ua.VariantType.Double),
                False,
                True,
            ),
            (
                "Sim.TestVarEUInformation",
                sim,
                ua.Variant(eu, ua.VariantType.ExtensionObject),
                False,
                False,
            ),
        ]

        for name, parent, value, writable, fixed_time in variable_nodes:
            node_id = ua.NodeId(name, self.idx, ua.NodeIdType.String)
            var = await parent.add_variable(node_id, name, value)
            if writable:
                await var.set_writable()
            if fixed_time:
                await var.write_value(
                    ua.DataValue(
                        Value=value,
                        StatusCode=ua.StatusCode(ua.StatusCodes.Good),
                        SourceTimestamp=self.fixed_time,
                        ServerTimestamp=self.fixed_time,
                    )
                )

        async with self:
            ramp_task = asyncio.create_task(self._ramp_loop())

            try:
                self.ready.set()
                await self.stop_requested.wait()
            finally:
                ramp_task.cancel()
                try:
                    await ramp_task
                except asyncio.CancelledError:
                    pass
