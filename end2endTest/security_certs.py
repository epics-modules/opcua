"""Self-signed certificates for the OPC UA security end-to-end tests.

Creates (in a given directory) the certificates and keys needed to run
the test server and the IOC with message security and user authentication:

* the server application instance certificate
* the client (IOC) application instance certificate
* a client certificate that is not trusted by the server
* the X.509 user identity certificate
  (its private key also in password-protected versions)

plus the client-side PKI directory structure (trusting the server certificate)
that is used with ``opcuaSetupPKI``.
"""

import datetime
import ipaddress
import socket
from dataclasses import dataclass
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID

SERVER_URI = "urn:epics-controls.org:OpcUa:e2e-test-server"
USER_KEY_PASSWORD = "e2e-key-secret"
IOC_NAME = "opcuaSecTestIoc"


def ioc_application_uri(ioc_name: str = IOC_NAME) -> str:
    """Application URI the device support uses (see devOpcuaSup/Session.h)."""
    return f"urn:{ioc_name}@{socket.gethostname()}:EPICS:IOC"


@dataclass(frozen=True)
class CertPair:
    cert_der: Path
    cert_pem: Path
    key_pem: Path
    crl_der: Path  # empty revocation list issued by this (self-signed) certificate

    def der_bytes(self) -> bytes:
        return self.cert_der.read_bytes()


@dataclass(frozen=True)
class SecurityTestCertificates:
    server: CertPair
    client: CertPair
    untrusted_client: CertPair
    user: CertPair
    # user private key protected with USER_KEY_PASSWORD, by PEM format
    user_encrypted_keys: dict[str, Path]
    client_pki: Path  # trusts the server certificate
    empty_client_pki: Path  # trusts nothing
    server_trusted_clients: Path  # trusted client certificates (server side)


def _write_pair(directory: Path, name: str, cert: x509.Certificate,
                key: rsa.RSAPrivateKey) -> CertPair:
    pair = CertPair(directory / f"{name}.der",
                    directory / f"{name}.pem",
                    directory / f"{name}_key.pem",
                    directory / f"{name}.crl")
    pair.cert_der.write_bytes(cert.public_bytes(serialization.Encoding.DER))
    pair.cert_pem.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    pair.key_pem.write_bytes(key.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.TraditionalOpenSSL,
        serialization.NoEncryption()))
    pair.key_pem.chmod(0o600)
    now = datetime.datetime.now(datetime.UTC)
    crl = (
        x509.CertificateRevocationListBuilder()
        .issuer_name(cert.subject)
        .last_update(now - datetime.timedelta(days=1))
        .next_update(now + datetime.timedelta(days=30))
        .add_extension(x509.CRLNumber(1), critical=False)
        .sign(key, hashes.SHA256())
    )
    pair.crl_der.write_bytes(crl.public_bytes(serialization.Encoding.DER))
    return pair


def _write_encrypted_keys(pair: CertPair, password: str) -> dict[str, Path]:
    """Write the private key of a pair, protected with a password, in both PEM formats."""
    key = serialization.load_pem_private_key(pair.key_pem.read_bytes(), password=None)
    formats = {
        "pkcs8": serialization.PrivateFormat.PKCS8,  # BEGIN ENCRYPTED PRIVATE KEY
        "traditional": serialization.PrivateFormat.TraditionalOpenSSL,  # Proc-Type: 4,ENCRYPTED
    }
    keys = {}
    for name, fmt in formats.items():
        path = pair.key_pem.with_name(f"{pair.key_pem.stem}_{name}_encrypted.pem")
        path.write_bytes(key.private_bytes(
            serialization.Encoding.PEM, fmt,
            serialization.BestAvailableEncryption(password.encode())))
        path.chmod(0o600)
        keys[name] = path
    return keys


def _self_signed(common_name: str, application_uri: str | None,
                 ext_key_usage: list[x509.ObjectIdentifier]) -> tuple[x509.Certificate, rsa.RSAPrivateKey]:
    """Create a self-signed OPC UA application instance certificate.

    Follows the certificate profile described in
    documentation/how-to/security_configuration.md.
    """
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    name = x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, common_name),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, "EPICS opcua e2e tests"),
    ])
    now = datetime.datetime.now(datetime.UTC)
    builder = (
        x509.CertificateBuilder()
        .subject_name(name)
        .issuer_name(name)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - datetime.timedelta(days=1))
        .not_valid_after(now + datetime.timedelta(days=30))
        .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
        .add_extension(x509.KeyUsage(
            digital_signature=True, content_commitment=True,
            key_encipherment=True, data_encipherment=True,
            key_agreement=False, key_cert_sign=True, crl_sign=True,
            encipher_only=False, decipher_only=False), critical=True)
        .add_extension(x509.ExtendedKeyUsage(ext_key_usage), critical=True)
        .add_extension(x509.SubjectKeyIdentifier.from_public_key(key.public_key()),
                       critical=False)
    )
    if application_uri is not None:
        hostname = socket.gethostname()
        builder = builder.add_extension(x509.SubjectAlternativeName([
            x509.UniformResourceIdentifier(application_uri),
            x509.DNSName(hostname),
            x509.DNSName("localhost"),
            x509.IPAddress(ipaddress.IPv4Address("127.0.0.1")),
        ]), critical=False)
    return builder.sign(key, hashes.SHA256()), key


def _make_pki(root: Path, trusted: list[CertPair]) -> Path:
    """Standard PKI directory layout as expected by opcuaSetupPKI.

    Each trusted (self-signed) certificate comes with its (empty) CRL,
    as some PKI implementations (e.g. the UA SDK's) require a CRL for every CA.
    """
    for sub in ("trusted/certs", "trusted/crl", "issuers/certs", "issuers/crl"):
        (root / sub).mkdir(parents=True, exist_ok=True)
    for pair in trusted:
        (root / "trusted" / "certs" / pair.cert_der.name).write_bytes(pair.der_bytes())
        (root / "trusted" / "crl" / pair.crl_der.name).write_bytes(pair.crl_der.read_bytes())
    return root


def create_test_certificates(directory: Path) -> SecurityTestCertificates:
    directory.mkdir(parents=True, exist_ok=True)
    both = [ExtendedKeyUsageOID.SERVER_AUTH, ExtendedKeyUsageOID.CLIENT_AUTH]

    server = _write_pair(directory, "server",
                         *_self_signed("EPICS opcua e2e test server", SERVER_URI, both))
    client = _write_pair(directory, "client",
                         *_self_signed("EPICS opcua e2e test IOC", ioc_application_uri(), both))
    untrusted_client = _write_pair(directory, "untrusted_client",
                                   *_self_signed("Untrusted IOC", ioc_application_uri(), both))
    user = _write_pair(directory, "user",
                       *_self_signed("e2e-cert-user", None, [ExtendedKeyUsageOID.CLIENT_AUTH]))

    user_encrypted_keys = _write_encrypted_keys(user, USER_KEY_PASSWORD)

    # The open62541 client verifies the X.509 identity certificate against its PKI
    client_pki = _make_pki(directory / "pki", [server, user])
    empty_client_pki = _make_pki(directory / "pki_empty", [])
    server_trusted_clients = directory / "server_trusted"
    server_trusted_clients.mkdir(exist_ok=True)
    (server_trusted_clients / client.cert_der.name).write_bytes(client.der_bytes())

    return SecurityTestCertificates(server, client, untrusted_client, user,
                            user_encrypted_keys, client_pki, empty_client_pki, server_trusted_clients)
