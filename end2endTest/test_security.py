"""End-to-end tests for OPC UA security.

Message security (modes Sign and SignAndEncrypt with different security
policies), certificate trust (both directions, self-signed and CA-signed
certificates, incl. intermediate CAs and revocation) and user authentication
(user name/password and X.509 certificate identity tokens).

All certificates are created freshly for the test session.
The tests are skipped if the module was built without crypto support.
"""

import os
import re
from collections.abc import Callable, Generator
from contextlib import ExitStack
from dataclasses import dataclass
from pathlib import Path

import pytest
from asyncua import ua
from epics import caput

from opcua_test_server import OpcuaTestServer, ServerSecurity
from security_certs import (
    IOC_NAME,
    SERVER_URI,
    USER_KEY_PASSWORD,
    CertPair,
    SecurityTestCertificates,
    create_test_certificates,
)
from test_opcua import DiagIOC, caget, wait_for_change, wait_for_server_value, wait_for_value

REPO_ROOT = Path(__file__).resolve().parents[1]

USER_NAME = "e2e-user"
USER_PASSWORD = "e2e-secret"
CERT_USER_NAME = "e2e-cert-user"

# Time allowed for connecting (incl. security handshake) and setting up the subscription
CONNECT_TIMEOUT = 20.0
# Time to wait for a connection that must not happen
NO_CONNECT_WAIT = 8.0


def client_build_config() -> tuple[str, bool]:
    """Client library and crypto support of the module build (from cfg/CONFIG_OPCUA)."""
    config = REPO_ROOT / "cfg" / "CONFIG_OPCUA"
    if not config.exists():
        return "unknown", False
    text = config.read_text()
    client = re.search(r"^CLIENT\s*=\s*(\S+)", text, re.MULTILINE)
    client = client.group(1) if client else "unknown"
    crypto = re.search(rf"^{client}_USE_CRYPTO\s*=\s*YES\b", text, re.MULTILINE)
    return client, crypto is not None


def open62541_version() -> tuple[int, int] | None:
    """Version (major, minor) of the open62541 library the module was built against."""
    config = REPO_ROOT / "cfg" / "CONFIG_OPCUA"
    if not config.exists():
        return None
    top = re.search(r"^OPEN62541\s*=\s*(\S+)", config.read_text(), re.MULTILINE)
    if not top:
        return None
    top = Path(top.group(1))
    for header in (top / "include" / "open62541" / "config.h",
                   top / "include" / "open62541.h", top / "open62541.h"):
        if header.exists():
            text = header.read_text(errors="replace")
            major = re.search(r"#define\s+UA_OPEN62541_VER_MAJOR\s+(\d+)", text)
            minor = re.search(r"#define\s+UA_OPEN62541_VER_MINOR\s+(\d+)", text)
            if major and minor:
                return int(major.group(1)), int(minor.group(1))
    return None


CLIENT, HAS_CRYPTO = client_build_config()
OPEN62541_VERSION = open62541_version() if CLIENT == "OPEN62541" else None

# open62541 1.2 rejects any certificate with both keyCertSign and cRLSign key usage
# (as a CA certificate) when establishing a secure channel. The self-signed test
# certificates need these to sign their own CRLs (required by the UA SDK and by
# CRL checking in open62541 1.3), so the IOC cannot connect to the test server.
# (Tests expecting a rejection still pass.)
xfail_open62541_1_2 = pytest.mark.xfail(
    OPEN62541_VERSION is not None and OPEN62541_VERSION < (1, 3),
    reason="open62541 1.2 client rejects the self-signed test certificates "
           "(keyCertSign + cRLSign key usage)",
    strict=False)

pytestmark = pytest.mark.skipif(
    not HAS_CRYPTO, reason="module built without crypto support (no OPC UA security)")


@dataclass
class IocSecurity:
    """Security setup of the IOC (iocsh commands and session options)."""

    mode: str = "SignAndEncrypt"
    policy: str | None = None
    client_cert: CertPair | None = None
    pki: Path | None = None
    identity: dict[str, str] | None = None  # contents of the sec-id file


def write_startup_script(directory: Path, sec: IocSecurity) -> Path:
    lines = [
        'cd "$(OPCUA_TEST_TOP)"',
        'dbLoadDatabase("dbd/opcuaTestIoc.dbd")',
        "opcuaTestIoc_registerRecordDeviceDriver(pdbbase)",
        f'opcuaSaveRejected("{directory / "rejected"}")',
    ]
    if sec.pki is not None:
        lines.append(f'opcuaSetupPKI("{sec.pki}")')
    if sec.client_cert is not None:
        lines.append(f'opcuaClientCertificate("{sec.client_cert.cert_der}", '
                     f'"{sec.client_cert.key_pem}")')
    lines += [
        'opcuaSession("OPC1", "opc.tcp://127.0.0.1:4840")',
        'opcuaSubscription("SUB1", "OPC1", 100)',
        f'opcuaOptions("OPC1", "sec-mode={sec.mode}")',
    ]
    if sec.policy is not None:
        lines.append(f'opcuaOptions("OPC1", "sec-policy={sec.policy}")')
    if sec.identity is not None:
        id_file = directory / "identity"
        id_file.write_text("".join(f"{k}={v}\n" for k, v in sec.identity.items()))
        id_file.chmod(0o600)
        lines.append(f'opcuaOptions("OPC1", "sec-id={id_file}")')
    lines += [
        'opcuaOptions("OPC1", "debug=$(OPCUA_DEBUG=0)")',
        'dbLoadRecords("end2endTest/ioc/test_security.db", "OPCSUB=SUB1,NS=2")',
        "iocInit()",
    ]
    script = directory / "st_security.cmd"
    script.write_text("\n".join(lines) + "\n")
    return script


@pytest.fixture(scope="session")
def certs(tmp_path_factory) -> SecurityTestCertificates:
    return create_test_certificates(tmp_path_factory.mktemp("opcua-certs"))


@pytest.fixture
def default_server_security(certs) -> ServerSecurity:
    """Server offering Sign and SignAndEncrypt endpoints (no None endpoint).

    Only the IOC's client certificate is trusted.
    """
    return ServerSecurity(
        certificate=certs.server.cert_der,
        private_key=certs.server.key_pem,
        application_uri=SERVER_URI,
        trusted_clients=certs.server_trusted_clients,
        users={USER_NAME: USER_PASSWORD},
        certificate_users={certs.user.der_bytes(): CERT_USER_NAME},
    )


@pytest.fixture
def default_ioc_security(certs) -> IocSecurity:
    """IOC with trusted client certificate, trusting the server certificate."""
    return IocSecurity(client_cert=certs.client, pki=certs.client_pki)


@pytest.fixture
def start_secure(monkeypatch, tmp_path) -> Generator[Callable[[ServerSecurity, IocSecurity],
                                                     tuple[OpcuaTestServer, DiagIOC]]]:
    """Factory starting the test server and the IOC with the given security setup."""
    monkeypatch.setenv("OPCUA_TEST_TOP", str(REPO_ROOT))
    monkeypatch.setenv("OPCUA_DEBUG", os.environ.get("OPCUA_DEBUG", "1"))
    # Part of the IOC's application URI (must match the client certificate)
    monkeypatch.setenv("IOC", IOC_NAME)
    executable = REPO_ROOT / "bin" / os.environ["EPICS_HOST_ARCH"] / "opcuaTestIoc"

    iocs = []
    with ExitStack() as stack:
        def start(server_sec: ServerSecurity, ioc_sec: IocSecurity) -> tuple[OpcuaTestServer, DiagIOC]:
            server = stack.enter_context(OpcuaTestServer(security=server_sec))
            ioc = DiagIOC(str(write_startup_script(tmp_path, ioc_sec)), executable=str(executable))
            iocs.append(ioc)
            stack.enter_context(ioc)
            return server, ioc

        try:
            yield start
        finally:
            # pytest only shows this for failing tests
            for ioc in iocs:
                print("=== IOC output:")
                print(ioc.output)


def wait_until_connected(server: OpcuaTestServer) -> dict:
    """Wait for the IOC to be connected with monitored items established.

    Returns the server-side view of the session.
    """
    # SecLiveCheck only gets its value through the subscription
    assert wait_for_value("SecLiveCheck", 42, timeout=CONNECT_TIMEOUT), \
        "IOC did not connect (no subscription update for SecLiveCheck)"
    sessions = server.client_sessions()
    assert len(sessions) == 1, f"expected one client session, got {sessions}"
    return sessions[0]


def assert_not_connected(server: OpcuaTestServer, ioc: DiagIOC) -> None:
    """The IOC must not get connected (records stay disconnected)."""
    assert not wait_for_value("SecLiveCheck", 42, timeout=NO_CONNECT_WAIT), \
        "IOC connected although it should have been rejected"
    assert caget("SecInt32.SEVR") == 3  # INVALID
    assert server.client_sessions() == []
    assert ioc.is_running()


def check_read_write_monitor(server: OpcuaTestServer) -> None:
    """Basic functionality over the established (secure) session."""
    # Read (initial values)
    assert caget("SecInt32") == -2147483648
    assert caget("SecString") == "TestString01"
    assert caget("SecInt32.SEVR") == 0

    # Write (and get the update back through the subscription)
    nodeid = f"ns={server.idx};s=Sim.TestVarInt32"
    assert caput("SecInt32Out", 123456, wait=True)
    assert wait_for_server_value(server, nodeid, 123456, timeout=5.0)
    assert wait_for_value("SecInt32", 123456, timeout=5.0)

    assert caput("SecStringOut", "Secure write", wait=True)
    assert wait_for_server_value(server, f"ns={server.idx};s=Sim.TestVarString",
                                 "Secure write", timeout=5.0)
    assert wait_for_value("SecString", "Secure write", timeout=5.0)

    # Monitor (server-side change reaches the IOC)
    server.write_server_value(nodeid, -42)
    assert wait_for_value("SecInt32", -42, timeout=5.0)

    # Continuous subscription updates
    prev = caget("SecRamp")
    now = wait_for_change("SecRamp", prev, timeout=5.0)
    assert now != prev, "no subscription updates for SecRamp"


MODES = {
    "Sign": ua.MessageSecurityMode.Sign,
    "SignAndEncrypt": ua.MessageSecurityMode.SignAndEncrypt,
}
POLICIES = ["Basic256Sha256", "Aes128_Sha256_RsaOaep"]


class TestMessageSecurity:
    @xfail_open62541_1_2
    @pytest.mark.parametrize("policy", POLICIES)
    @pytest.mark.parametrize("mode", MODES)
    def test_secure_session(self, start_secure, default_server_security,
                            default_ioc_security, mode, policy) -> None:
        default_ioc_security.mode = mode
        default_ioc_security.policy = policy
        server, ioc = start_secure(default_server_security, default_ioc_security)

        session = wait_until_connected(server)
        assert session["mode"] == MODES[mode]
        assert session["policy"] == policy
        assert session["user"] == "anonymous"

        check_read_write_monitor(server)

        ioc.exit()
        assert not ioc.is_running()
        assert server.exception is None

    @xfail_open62541_1_2
    @pytest.mark.parametrize("mode", MODES)
    def test_mode_with_any_policy(self, start_secure, default_server_security,
                                  default_ioc_security, mode) -> None:
        default_ioc_security.mode = mode
        server, _ = start_secure(default_server_security, default_ioc_security)

        session = wait_until_connected(server)
        assert session["mode"] == MODES[mode]
        assert session["policy"] in POLICIES

    @xfail_open62541_1_2
    def test_best_mode(self, start_secure, default_server_security,
                       default_ioc_security) -> None:
        default_ioc_security.mode = "best"
        server, _ = start_secure(default_server_security, default_ioc_security)

        session = wait_until_connected(server)
        assert session["mode"] == ua.MessageSecurityMode.SignAndEncrypt
        check_read_write_monitor(server)

    @xfail_open62541_1_2
    def test_reconnect(self, start_secure, default_server_security,
                       default_ioc_security) -> None:
        server, ioc = start_secure(default_server_security, default_ioc_security)
        wait_until_connected(server)

        server.disconnect_from_clients()
        assert wait_for_value("SecInt32.SEVR", 3, timeout=20.0)  # INVALID
        server.reconnect_to_clients()
        assert wait_for_value("SecInt32.SEVR", 0, timeout=20.0)  # NO_ALARM

        session = wait_until_connected(server)
        assert session["mode"] == ua.MessageSecurityMode.SignAndEncrypt
        prev = caget("SecRamp")
        assert wait_for_change("SecRamp", prev, timeout=10.0) != prev, \
            "no subscription updates after reconnect"

        ioc.exit()
        assert not ioc.is_running()

    def test_no_security_rejected(self, start_secure, default_server_security,
                                  default_ioc_security) -> None:
        # The server does not offer an endpoint without security
        default_ioc_security.mode = "None"
        server, ioc = start_secure(default_server_security, default_ioc_security)
        assert_not_connected(server, ioc)

    def test_unavailable_policy_rejected(self, start_secure, default_server_security,
                                         default_ioc_security) -> None:
        default_server_security.policies = [ua.SecurityPolicyType.Basic256Sha256_SignAndEncrypt]
        default_ioc_security.policy = "Aes128_Sha256_RsaOaep"
        server, ioc = start_secure(default_server_security, default_ioc_security)
        assert_not_connected(server, ioc)


class TestCertificateTrust:
    def test_untrusted_server_rejected(self, start_secure, default_server_security,
                                       default_ioc_security, certs) -> None:
        # The IOC's PKI does not contain the server certificate
        default_ioc_security.pki = certs.empty_client_pki
        server, ioc = start_secure(default_server_security, default_ioc_security)
        assert_not_connected(server, ioc)

    def test_untrusted_client_rejected(self, start_secure, default_server_security,
                                       default_ioc_security, certs) -> None:
        # The server does not trust the IOC's client certificate
        default_ioc_security.client_cert = certs.untrusted_client
        server, ioc = start_secure(default_server_security, default_ioc_security)
        assert_not_connected(server, ioc)


class TestCaSignedCertificates:
    """Server and client certificates issued by a CA; only the CA is trusted."""

    @pytest.fixture
    def ca_server_security(self, default_server_security, certs) -> ServerSecurity:
        """Server with a CA-signed certificate, trusting (only) the root CA."""
        default_server_security.certificate = certs.ca.server.cert_der
        default_server_security.private_key = certs.ca.server.key_pem
        default_server_security.trusted_clients = certs.ca.server_trusted
        default_server_security.client_crls = certs.ca.server_crls
        return default_server_security

    @pytest.fixture
    def ca_ioc_security(self, certs) -> IocSecurity:
        """IOC with a CA-signed client certificate, trusting (only) the root CA."""
        return IocSecurity(client_cert=certs.ca.client, pki=certs.ca.client_pki)

    @pytest.mark.parametrize("mode", MODES)
    def test_ca_signed_session(self, start_secure, ca_server_security,
                               ca_ioc_security, mode) -> None:
        ca_ioc_security.mode = mode
        server, ioc = start_secure(ca_server_security, ca_ioc_security)

        session = wait_until_connected(server)
        assert session["mode"] == MODES[mode]
        check_read_write_monitor(server)

        ioc.exit()
        assert not ioc.is_running()
        assert server.exception is None

    def test_reconnect(self, start_secure, ca_server_security, ca_ioc_security) -> None:
        server, ioc = start_secure(ca_server_security, ca_ioc_security)
        wait_until_connected(server)

        server.disconnect_from_clients()
        assert wait_for_value("SecInt32.SEVR", 3, timeout=20.0)  # INVALID
        server.reconnect_to_clients()
        assert wait_for_value("SecInt32.SEVR", 0, timeout=20.0)  # NO_ALARM

        wait_until_connected(server)
        prev = caget("SecRamp")
        assert wait_for_change("SecRamp", prev, timeout=10.0) != prev, \
            "no subscription updates after reconnect"

        ioc.exit()
        assert not ioc.is_running()

    def test_server_from_intermediate_ca(self, start_secure, ca_server_security,
                                         ca_ioc_security, certs) -> None:
        # The server only sends its own certificate; the IOC builds the chain
        # using the intermediate CA certificate from the PKI's issuers list
        ca_server_security.certificate = certs.ca.server_from_intermediate.cert_der
        ca_server_security.private_key = certs.ca.server_from_intermediate.key_pem
        ca_ioc_security.pki = certs.ca.client_pki_intermediate
        server, ioc = start_secure(ca_server_security, ca_ioc_security)

        wait_until_connected(server)
        check_read_write_monitor(server)

        ioc.exit()
        assert not ioc.is_running()

    def test_server_from_intermediate_ca_without_issuer_rejected(
            self, start_secure, ca_server_security, ca_ioc_security, certs) -> None:
        # The IOC trusts the root CA, but cannot build the chain
        # (intermediate CA certificate missing)
        ca_server_security.certificate = certs.ca.server_from_intermediate.cert_der
        ca_server_security.private_key = certs.ca.server_from_intermediate.key_pem
        server, ioc = start_secure(ca_server_security, ca_ioc_security)
        assert_not_connected(server, ioc)

    def test_server_from_unknown_ca_rejected(self, start_secure, ca_server_security,
                                             ca_ioc_security, certs) -> None:
        # The IOC trusts only an unrelated CA
        ca_ioc_security.pki = certs.ca.client_pki_other_ca
        server, ioc = start_secure(ca_server_security, ca_ioc_security)
        assert_not_connected(server, ioc)

    def test_server_ca_only_in_issuers_rejected(self, start_secure, ca_server_security,
                                                ca_ioc_security, certs) -> None:
        # The root CA is known (issuers list), but not trusted
        ca_ioc_security.pki = certs.ca.client_pki_issuers_only
        server, ioc = start_secure(ca_server_security, ca_ioc_security)
        assert_not_connected(server, ioc)

    def test_revoked_server_rejected(self, start_secure, ca_server_security,
                                     ca_ioc_security, certs) -> None:
        # The root CA's CRL (in the IOC's PKI) revokes the server certificate
        ca_ioc_security.pki = certs.ca.client_pki_server_revoked
        server, ioc = start_secure(ca_server_security, ca_ioc_security)
        assert_not_connected(server, ioc)

    def test_client_from_untrusted_ca_rejected(self, start_secure, ca_server_security,
                                               ca_ioc_security, certs) -> None:
        # The IOC's client certificate is issued by a CA the server does not trust
        ca_ioc_security.client_cert = certs.ca.client_from_other_ca
        server, ioc = start_secure(ca_server_security, ca_ioc_security)
        assert_not_connected(server, ioc)

    def test_revoked_client_rejected(self, start_secure, ca_server_security,
                                     ca_ioc_security, certs) -> None:
        # The root CA's CRL (on the server) revokes the IOC's client certificate
        ca_server_security.client_crls = certs.ca.server_crls_client_revoked
        server, ioc = start_secure(ca_server_security, ca_ioc_security)
        assert_not_connected(server, ioc)


class TestUserAuthentication:
    @pytest.fixture
    def username_only(self, default_server_security) -> ServerSecurity:
        default_server_security.identity_tokens = [ua.UserNameIdentityToken]
        return default_server_security

    @pytest.fixture
    def x509_only(self, default_server_security) -> ServerSecurity:
        default_server_security.identity_tokens = [ua.X509IdentityToken]
        return default_server_security

    @xfail_open62541_1_2
    @pytest.mark.parametrize("mode", MODES)
    def test_username_password(self, start_secure, username_only,
                               default_ioc_security, mode) -> None:
        default_ioc_security.mode = mode
        default_ioc_security.identity = {"user": USER_NAME, "pass": USER_PASSWORD}
        server, ioc = start_secure(username_only, default_ioc_security)

        session = wait_until_connected(server)
        assert session["user"] == USER_NAME
        assert session["mode"] == MODES[mode]
        check_read_write_monitor(server)

        ioc.exit()
        assert not ioc.is_running()

    def test_wrong_password_rejected(self, start_secure, username_only,
                                     default_ioc_security) -> None:
        default_ioc_security.identity = {"user": USER_NAME, "pass": "wrong-password"}
        server, ioc = start_secure(username_only, default_ioc_security)
        assert_not_connected(server, ioc)

    def test_unknown_user_rejected(self, start_secure, username_only,
                                   default_ioc_security) -> None:
        default_ioc_security.identity = {"user": "nobody", "pass": USER_PASSWORD}
        server, ioc = start_secure(username_only, default_ioc_security)
        assert_not_connected(server, ioc)

    def test_anonymous_rejected(self, start_secure, username_only,
                                default_ioc_security) -> None:
        # Server requires authentication, IOC has no identity configured
        server, ioc = start_secure(username_only, default_ioc_security)
        assert_not_connected(server, ioc)

    @pytest.mark.xfail(
        CLIENT == "OPEN62541",
        reason="open62541 client: open62541 1.3 does not send the userTokenSignature "
               "(proof of possession of the user's private key)",
        strict=False)
    @pytest.mark.parametrize("mode", MODES)
    def test_x509_certificate(self, start_secure, x509_only,
                              default_ioc_security, certs, mode) -> None:
        default_ioc_security.mode = mode
        default_ioc_security.identity = {"cert": str(certs.user.cert_der),
                                         "key": str(certs.user.key_pem)}
        server, ioc = start_secure(x509_only, default_ioc_security)

        session = wait_until_connected(server)
        assert session["user"] == CERT_USER_NAME
        assert session["mode"] == MODES[mode]
        check_read_write_monitor(server)

        ioc.exit()
        assert not ioc.is_running()

    @pytest.mark.xfail(
        OPEN62541_VERSION is not None and OPEN62541_VERSION < (1, 4),
        reason="open62541 1.3 and older does not send the userTokenSignature "
               "(proof of possession of the user's private key)",
        strict=False)
    @pytest.mark.parametrize("key_format", ["pkcs8", "traditional"])
    def test_x509_encrypted_key(self, start_secure, x509_only,
                                default_ioc_security, certs, key_format) -> None:
        # Password-protected private key, password in the identity file
        default_ioc_security.identity = {"cert": str(certs.user.cert_der),
                                         "key": str(certs.user_encrypted_keys[key_format]),
                                         "pass": USER_KEY_PASSWORD}
        server, ioc = start_secure(x509_only, default_ioc_security)

        session = wait_until_connected(server)
        assert session["user"] == CERT_USER_NAME
        check_read_write_monitor(server)

        ioc.exit()
        assert not ioc.is_running()

    def test_x509_encrypted_key_wrong_password_rejected(self, start_secure, x509_only,
                                                        default_ioc_security, certs) -> None:
        default_ioc_security.identity = {"cert": str(certs.user.cert_der),
                                         "key": str(certs.user_encrypted_keys["pkcs8"]),
                                         "pass": "wrong-password"}
        server, ioc = start_secure(x509_only, default_ioc_security)
        assert_not_connected(server, ioc)

    def test_x509_encrypted_key_no_password_rejected(self, start_secure, x509_only,
                                                     default_ioc_security, certs) -> None:
        default_ioc_security.identity = {"cert": str(certs.user.cert_der),
                                         "key": str(certs.user_encrypted_keys["pkcs8"])}
        server, ioc = start_secure(x509_only, default_ioc_security)
        assert_not_connected(server, ioc)

    def test_unknown_x509_certificate_rejected(self, start_secure, x509_only,
                                               default_ioc_security, certs) -> None:
        # A valid certificate (with key) that is not registered as a user on the server
        default_ioc_security.identity = {"cert": str(certs.untrusted_client.cert_der),
                                         "key": str(certs.untrusted_client.key_pem)}
        server, ioc = start_secure(x509_only, default_ioc_security)
        assert_not_connected(server, ioc)
