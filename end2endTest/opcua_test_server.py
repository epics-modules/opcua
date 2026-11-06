import asyncio
import logging
import threading
from dataclasses import dataclass, field
from datetime import UTC, datetime
from pathlib import Path
from types import TracebackType
from typing import Any, Self

from asyncua import Server, ua
from asyncua.crypto.permission_rules import User, UserRole
from asyncua.crypto.truststore import TrustStore
from asyncua.crypto.validator import CertificateValidator, CertificateValidatorOptions
from asyncua.server.internal_session import SessionState
from asyncua.server.user_managers import UserManager

NS_URI = "http://epics-controls.org/OpcUa"
RAMP_MAX_VALUE = 1000
RAMP_UPDATE_PERIOD_S = 1


@dataclass
class ServerSecurity:
    """Security configuration of the test server.

    Without it, the server only offers an endpoint without security
    and accepts any (anonymous) user.
    """

    certificate: Path  # server application instance certificate (DER)
    private_key: Path  # matching private key (PEM)
    application_uri: str  # must match the URI in the server certificate
    # Offered endpoints (message security mode + security policy)
    policies: list[ua.SecurityPolicyType] = field(default_factory=lambda: [
        ua.SecurityPolicyType.Basic256Sha256_Sign,
        ua.SecurityPolicyType.Basic256Sha256_SignAndEncrypt,
        ua.SecurityPolicyType.Aes128Sha256RsaOaep_Sign,
        ua.SecurityPolicyType.Aes128Sha256RsaOaep_SignAndEncrypt,
    ])
    # Directory with trusted client certificates
    # (None: accept any client certificate)
    trusted_clients: Path | None = None
    # Offered user identity token types
    identity_tokens: list[type] = field(default_factory=lambda: [
        ua.AnonymousIdentityToken, ua.UserNameIdentityToken, ua.X509IdentityToken,
    ])
    # Accepted users: username -> password
    users: dict[str, str] = field(default_factory=dict)
    # Accepted X.509 user certificates: DER encoded certificate -> username
    certificate_users: dict[bytes, str] = field(default_factory=dict)


class E2EUserManager(UserManager):
    """Checks user name/password and X.509 user certificates."""

    def __init__(self, security: ServerSecurity) -> None:  # noqa: D107
        self.security = security

    def get_user(self, iserver: Any, username: str | None = None,
                 password: str | None = None, certificate: Any = None) -> User | None:
        if username is not None:
            if self.security.users.get(username) != password:
                return None
            return User(role=UserRole.User, name=username)
        # asyncua passes the X.509 user certificate for X509IdentityTokens,
        # but the client's application certificate for all other tokens
        name = self.security.certificate_users.get(certificate)
        if name is not None:
            return User(role=UserRole.User, name=name)
        if ua.AnonymousIdentityToken in self.security.identity_tokens:
            return User(role=UserRole.User, name="anonymous")
        return None


class OpcuaTestServer(Server):
    def __init__( # noqa: D107
        self,
        endpoint: str = "opc.tcp://0.0.0.0:4840",
        server_name: str = "EPICS OPCUA asyncua test server",
        security: ServerSecurity | None = None,
    ) -> None:
        self.security = security
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
        # Simulate a server restart: also drop all client sessions (and their
        # subscriptions), so that clients have to create new sessions.
        # Otherwise asyncua keeps sessions with subscriptions alive for
        # re-activation on a new connection, which does not work reliably
        # (e.g. its Republish responses do not follow the spec).
        sessions = getattr(self.iserver, "_external_sessions", {})
        for session in list(sessions.values()):
            await session.close_session(delete_subs=True)

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

    def client_sessions(self) -> list[dict[str, Any]]:
        """Security settings and user of all activated client sessions."""
        async def _sessions():
            result = []
            for protocol in list(self.bserver.clients):
                processor = protocol.processor
                session = processor.session if processor is not None else None
                if session is None or session.state != SessionState.Activated:
                    continue
                policy = processor._connection.security_policy
                result.append({
                    "policy": policy.URI.rsplit("#", 1)[-1],
                    "mode": policy.Mode,
                    "user": session.user.name if session.user is not None else None,
                })
            return result

        fut = asyncio.run_coroutine_threadsafe(_sessions(), self.loop)
        return fut.result(timeout=10)

    async def _setup_security(self) -> None:
        sec = self.security
        await self.set_application_uri(sec.application_uri)
        await self.load_certificate(str(sec.certificate))
        await self.load_private_key(str(sec.private_key))
        self.set_security_policy(sec.policies)
        self.set_identity_tokens(sec.identity_tokens)
        self.iserver.set_user_manager(E2EUserManager(sec))
        if sec.trusted_clients is not None:
            trust_store = TrustStore([sec.trusted_clients], [])
            await trust_store.load()
            self.set_certificate_validator(CertificateValidator(
                CertificateValidatorOptions.TRUSTED_VALIDATION
                | CertificateValidatorOptions.PEER_CLIENT,
                trust_store))

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
        if self.security is not None:
            await self._setup_security()

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
            (
                "Sim.LastPVLiveCheck",
                sim,
                ua.Variant(42, ua.VariantType.UInt32),
                True,
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
