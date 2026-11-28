"""Certificates for the OPC UA security end-to-end tests.

Creates (in a given directory) the certificates and keys needed to run
the test server and the IOC with message security and user authentication.

Self-signed:

* the server application instance certificate
* the client (IOC) application instance certificate
* a client certificate that is not trusted by the server
* the X.509 user identity certificate
  (its private key also in password-protected versions)

plus the client-side PKI directory structure (trusting the server certificate)
that is used with ``opcuaSetupPKI``.

CA-signed (see ``CaTestCertificates``):

* a root CA, an intermediate CA (issued by the root CA) and an unrelated CA
* server and client application instance certificates issued by these CAs
* CRLs (empty, and revoking the server or client certificate)

plus client-side PKI directory structures and server-side trust lists
that trust only the CAs.
"""

import dataclasses
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
    # Empty revocation list issued by this certificate
    # (self-signed and CA certificates only, None for CA-signed application certificates)
    crl_der: Path | None

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
    ca: "CaTestCertificates"


@dataclass(frozen=True)
class CaTestCertificates:
    """CA-signed certificates and the matching trust configurations."""

    root_ca: CertPair
    intermediate_ca: CertPair  # issued by root_ca
    other_ca: CertPair  # unrelated CA, not trusted anywhere
    server: CertPair  # issued by root_ca
    client: CertPair  # issued by root_ca
    server_from_intermediate: CertPair  # issued by intermediate_ca
    client_from_other_ca: CertPair  # issued by other_ca

    # IOC side (opcuaSetupPKI)
    client_pki: Path  # trusts root_ca
    client_pki_intermediate: Path  # trusts root_ca, intermediate_ca in issuers
    client_pki_issuers_only: Path  # root_ca only in issuers (not trusted)
    client_pki_other_ca: Path  # trusts other_ca
    client_pki_server_revoked: Path  # trusts root_ca, whose CRL revokes server

    # Server side
    server_trusted: Path  # trusted CA certificates (root_ca)
    server_crls: Path  # root_ca's (empty) CRL
    server_crls_client_revoked: Path  # root_ca's CRL revoking client


def _crl(issuer_cert: x509.Certificate, issuer_key: rsa.RSAPrivateKey,
         revoked: list[x509.Certificate] = (), number: int = 1) -> x509.CertificateRevocationList:
    """Revocation list issued by the given (self-signed or CA) certificate."""
    now = datetime.datetime.now(datetime.UTC)
    builder = (
        x509.CertificateRevocationListBuilder()
        .issuer_name(issuer_cert.subject)
        .last_update(now - datetime.timedelta(days=1))
        .next_update(now + datetime.timedelta(days=30))
        .add_extension(x509.CRLNumber(number), critical=False)
        .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(
            issuer_key.public_key()), critical=False)
    )
    for cert in revoked:
        builder = builder.add_revoked_certificate(
            x509.RevokedCertificateBuilder()
            .serial_number(cert.serial_number)
            .revocation_date(now - datetime.timedelta(hours=1))
            .build())
    return builder.sign(issuer_key, hashes.SHA256())


def _write_pair(directory: Path, name: str, cert: x509.Certificate,
                key: rsa.RSAPrivateKey, with_crl: bool = True) -> CertPair:
    pair = CertPair(directory / f"{name}.der",
                    directory / f"{name}.pem",
                    directory / f"{name}_key.pem",
                    directory / f"{name}.crl" if with_crl else None)
    pair.cert_der.write_bytes(cert.public_bytes(serialization.Encoding.DER))
    pair.cert_pem.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    pair.key_pem.write_bytes(key.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.TraditionalOpenSSL,
        serialization.NoEncryption()))
    pair.key_pem.chmod(0o600)
    if pair.crl_der is not None:
        pair.crl_der.write_bytes(_crl(cert, key).public_bytes(serialization.Encoding.DER))
    return pair


def _load(pair: CertPair) -> tuple[x509.Certificate, rsa.RSAPrivateKey]:
    return (x509.load_der_x509_certificate(pair.cert_der.read_bytes()),
            serialization.load_pem_private_key(pair.key_pem.read_bytes(), password=None))


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


def _name(common_name: str) -> x509.Name:
    return x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, common_name),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, "EPICS opcua e2e tests"),
    ])


def _builder(subject: x509.Name, key: rsa.RSAPrivateKey,
             issuer: tuple[x509.Certificate, rsa.RSAPrivateKey] | None) -> x509.CertificateBuilder:
    """Certificate builder with the parts common to all test certificates."""
    now = datetime.datetime.now(datetime.UTC)
    builder = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(subject if issuer is None else issuer[0].subject)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - datetime.timedelta(days=1))
        .not_valid_after(now + datetime.timedelta(days=30))
        .add_extension(x509.SubjectKeyIdentifier.from_public_key(key.public_key()),
                       critical=False)
    )
    if issuer is not None:
        builder = builder.add_extension(
            x509.AuthorityKeyIdentifier.from_issuer_public_key(issuer[1].public_key()),
            critical=False)
    return builder


def _ca(common_name: str, issuer: tuple[x509.Certificate, rsa.RSAPrivateKey] | None = None
        ) -> tuple[x509.Certificate, rsa.RSAPrivateKey]:
    """Create a CA certificate (self-signed root CA, or issued by another CA)."""
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    builder = (
        _builder(_name(common_name), key, issuer)
        .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
        .add_extension(x509.KeyUsage(
            digital_signature=True, content_commitment=False,
            key_encipherment=False, data_encipherment=False,
            key_agreement=False, key_cert_sign=True, crl_sign=True,
            encipher_only=False, decipher_only=False), critical=True)
    )
    return builder.sign(key if issuer is None else issuer[1], hashes.SHA256()), key


def _app_cert(common_name: str, application_uri: str | None,
              ext_key_usage: list[x509.ObjectIdentifier],
              issuer: tuple[x509.Certificate, rsa.RSAPrivateKey] | None = None
              ) -> tuple[x509.Certificate, rsa.RSAPrivateKey]:
    """Create an OPC UA application instance certificate.

    Self-signed (issuer None) or issued by the given CA (certificate, key).
    Follows the certificate profile described in
    documentation/how-to/security_configuration.md.
    """
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    # Self-signed certificates need keyCertSign and cRLSign to sign their own CRL
    self_signed = issuer is None
    builder = (
        _builder(_name(common_name), key, issuer)
        .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
        .add_extension(x509.KeyUsage(
            digital_signature=True, content_commitment=True,
            key_encipherment=True, data_encipherment=True,
            key_agreement=False, key_cert_sign=self_signed, crl_sign=self_signed,
            encipher_only=False, decipher_only=False), critical=True)
        .add_extension(x509.ExtendedKeyUsage(ext_key_usage), critical=True)
    )
    if application_uri is not None:
        hostname = socket.gethostname()
        builder = builder.add_extension(x509.SubjectAlternativeName([
            x509.UniformResourceIdentifier(application_uri),
            x509.DNSName(hostname),
            x509.DNSName("localhost"),
            x509.IPAddress(ipaddress.IPv4Address("127.0.0.1")),
        ]), critical=False)
    return builder.sign(key if issuer is None else issuer[1], hashes.SHA256()), key


def _make_pki(root: Path, trusted: list[CertPair], issuers: list[CertPair] = ()) -> Path:
    """Standard PKI directory layout as expected by opcuaSetupPKI.

    Each trusted/issuer (self-signed or CA) certificate comes with its CRL,
    as some PKI implementations (e.g. the UA SDK's) require a CRL for every CA.
    """
    for sub in ("trusted/certs", "trusted/crl", "issuers/certs", "issuers/crl"):
        (root / sub).mkdir(parents=True, exist_ok=True)
    for kind, pairs in (("trusted", trusted), ("issuers", issuers)):
        for pair in pairs:
            (root / kind / "certs" / pair.cert_der.name).write_bytes(pair.der_bytes())
            (root / kind / "crl" / pair.crl_der.name).write_bytes(pair.crl_der.read_bytes())
    return root


def _write_crl(path: Path, issuer: CertPair, revoked: list[CertPair]) -> CertPair:
    """Write a CRL of a CA revoking the given certificates.

    Returns the CA's CertPair with this CRL (for use with _make_pki).
    """
    crl = _crl(*_load(issuer), revoked=[_load(pair)[0] for pair in revoked], number=2)
    path.write_bytes(crl.public_bytes(serialization.Encoding.DER))
    return dataclasses.replace(issuer, crl_der=path)


def _single_file_dir(directory: Path, name: str, content: bytes) -> Path:
    directory.mkdir(exist_ok=True)
    (directory / name).write_bytes(content)
    return directory


def create_ca_certificates(directory: Path) -> CaTestCertificates:
    directory.mkdir(parents=True, exist_ok=True)
    both = [ExtendedKeyUsageOID.SERVER_AUTH, ExtendedKeyUsageOID.CLIENT_AUTH]

    root_ca = _write_pair(directory, "root_ca", *_ca("EPICS opcua e2e test root CA"))
    intermediate_ca = _write_pair(directory, "intermediate_ca",
                                  *_ca("EPICS opcua e2e test intermediate CA", _load(root_ca)))
    other_ca = _write_pair(directory, "other_ca", *_ca("Other CA"))

    def issued(name: str, common_name: str, uri: str, ca: CertPair) -> CertPair:
        return _write_pair(directory, name,
                           *_app_cert(common_name, uri, both, _load(ca)), with_crl=False)

    server = issued("ca_server", "EPICS opcua e2e test server (CA-signed)", SERVER_URI, root_ca)
    client = issued("ca_client", "EPICS opcua e2e test IOC (CA-signed)",
                    ioc_application_uri(), root_ca)
    server_from_intermediate = issued("intermediate_server",
                                      "EPICS opcua e2e test server (intermediate CA)",
                                      SERVER_URI, intermediate_ca)
    client_from_other_ca = issued("other_ca_client", "IOC from other CA",
                                  ioc_application_uri(), other_ca)

    root_ca_server_revoked = _write_crl(directory / "root_ca_server_revoked.crl",
                                        root_ca, [server])
    root_ca_client_revoked = _write_crl(directory / "root_ca_client_revoked_crl.der",
                                        root_ca, [client])

    return CaTestCertificates(
        root_ca, intermediate_ca, other_ca,
        server, client, server_from_intermediate, client_from_other_ca,
        client_pki=_make_pki(directory / "ca_pki", [root_ca]),
        client_pki_intermediate=_make_pki(directory / "ca_pki_intermediate",
                                          [root_ca], [intermediate_ca]),
        client_pki_issuers_only=_make_pki(directory / "ca_pki_issuers_only", [], [root_ca]),
        client_pki_other_ca=_make_pki(directory / "ca_pki_other_ca", [other_ca]),
        client_pki_server_revoked=_make_pki(directory / "ca_pki_server_revoked",
                                            [root_ca_server_revoked]),
        server_trusted=_single_file_dir(directory / "ca_server_trusted",
                                        root_ca.cert_der.name, root_ca.der_bytes()),
        # asyncua's TrustStore only loads files named *der or *pem
        server_crls=_single_file_dir(directory / "ca_server_crls", "root_ca_crl.der",
                                     root_ca.crl_der.read_bytes()),
        server_crls_client_revoked=_single_file_dir(
            directory / "ca_server_crls_client_revoked", root_ca_client_revoked.crl_der.name,
            root_ca_client_revoked.crl_der.read_bytes()),
    )


def create_test_certificates(directory: Path) -> SecurityTestCertificates:
    directory.mkdir(parents=True, exist_ok=True)
    both = [ExtendedKeyUsageOID.SERVER_AUTH, ExtendedKeyUsageOID.CLIENT_AUTH]

    server = _write_pair(directory, "server",
                         *_app_cert("EPICS opcua e2e test server", SERVER_URI, both))
    client = _write_pair(directory, "client",
                         *_app_cert("EPICS opcua e2e test IOC", ioc_application_uri(), both))
    untrusted_client = _write_pair(directory, "untrusted_client",
                                   *_app_cert("Untrusted IOC", ioc_application_uri(), both))
    user = _write_pair(directory, "user",
                       *_app_cert("e2e-cert-user", None, [ExtendedKeyUsageOID.CLIENT_AUTH]))

    user_encrypted_keys = _write_encrypted_keys(user, USER_KEY_PASSWORD)

    # The open62541 client verifies the X.509 identity certificate against its PKI
    client_pki = _make_pki(directory / "pki", [server, user])
    empty_client_pki = _make_pki(directory / "pki_empty", [])
    server_trusted_clients = directory / "server_trusted"
    server_trusted_clients.mkdir(exist_ok=True)
    (server_trusted_clients / client.cert_der.name).write_bytes(client.der_bytes())

    return SecurityTestCertificates(server, client, untrusted_client, user,
                                    user_encrypted_keys, client_pki, empty_client_pki,
                                    server_trusted_clients,
                                    create_ca_certificates(directory / "ca"))
