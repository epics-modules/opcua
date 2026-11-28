# Configuring OPC UA Security

An IOC using the OPC UA Device Support
is considered to be a production system.
As such, it has to be fully configured and supplied with the certificates
needed to connect to the configured OPC UA servers.

## Certificate Store Setup

The iocShell command `opcuaSetupPKI`
sets the location(s) for the certificate store.
The IOC needs to have read access to all files in the PKI store,
further access should not be granted as this might compromise security.

In its simple form,
the single argument defines the location of the PKI certificate store,
using default subdirectories:

* Trusted peer and CA certificates: `trusted/certs`
* CA certificate revocation lists: `trusted/crl`
* Intermediate issuer certificates: `issuers/certs`
* Issuer certificate revocation lists: `issuers/crl`

In the fully detailed form (using four arguments),
the four locations are specified separately.

## Client Certificate (Client Authentication)

The iocShell command `opcuaClientCertificate`
sets the locations for the client certificate (PEM or DER format)
and the matching private key (PEM format).

## Session Security Setting

Two security-related session options are used
to configure the security features for a given OPC UA session,
by calling `opcuaSession` or `opcuaOptions` in the iocShell.

* `sec-mode`:
  selects the specified message security mode
  for the connection (`None`, `Sign`, `SignAndEncrypt`).
  The special keyword `best` (default) will have the IOC choose the best mode,
  based on the server-supplied security level.
* `sec-policy`:
  selects a specific policy by its short name (e.g., `Basic256Sha256`).

:::{note}
To connect without security, you have to explicitly set `sec-mode=None`.
:::

## Identity (Server-Side User Authentication)

Without configuration, an Anonymous Identity Token will be used.

To use a Username Identity or a Certificate Identity Token,
prepare an identity file
and configure the filename through the session option `sec-id`.

:::{attention}
If this file contains a (clear text) password,
its access right settings are crucial.
It needs to be on a local disk
and only readable by the user running the IOC.
:::

### Username Identity Token

In the identity file, set:

```
user=<username>
pass=<password>
```

### Certificate Identity Token

In the identity file, set:

```
cert=<certificate file>
key=<private key file>
pass=<password> (optional, if private key is protected)
```

## Managing Certificates

The `openssl` command line utility can be used
to convert certificates between formats.

**DER to PEM:**

```bash
openssl x509 -inform der -in <cert>.der -out <cert>.pem
```

**PEM to DER:**

```bash
openssl x509 -in <cert>.pem -outform der -out <cert>.der
```

*Note:*
DER format requires `.der` (certificates) or `.crl` (revocation lists) extensions.
PEM format requires `.pem` extension.

### Trusting a Server Certificate

1. Use `opcuaSaveRejected` to configure where to save rejected certificates.
2. Attempt a connection.
   The server certificate will be saved to the specified location.
3. Copy the certificate file to the IOC's certificate store
   under `trusted/certs` to explicitly trust it.
   :::{warning}
   Always verify the certificate's thumbprint before trusting it.
   :::

### Creating Application Instance Certificates

**NOTE:** open62541 version 1.3 does not support using a client certificate,
you need to use 1.4 or higher.

Creating a self-signed certificate for OPC UA use is pretty straight-forward.
Follow the documented procedure,
giving your certificate/key pair the following properties:

- Choose the issuer information to match your situation.
- Sign using `SHA 256` (i.e., `sha256WithRSAEncryption`).
- X509v3 Basic Constraints: **critical**, `CA:FALSE`.
- X509v3 Key Usage: **critical**, `Digital Signature`, `Non Repudiation`,
  `Key Encipherment`, `Data Encipherment`, `Certificate Sign`.
- X509v3 Extended Key Usage: **critical**, `TLS Web Server Authentication`,
  `TLS Web Client Authentication`.
- X509v3 Subject Alternative Name: `URI:urn:<ioc>@<host>:EPICS:IOC`,
  `DNS:<host>`
  with `<ioc>` being the IOC name and `<host>` being the hostname
  (i.e., the result of a `gethostname()` call) of the machine that runs the IOC.
  The URI tag *must* match
  what the Device Support module sets as its application URI.
  Alternatively, a numerical `IP Address` tag can be used instead of `DNS`.

A simple client certificate/key pair can also be created
using the `openssl` command line utility, e.g.:

```bash
openssl req -x509 -newkey rsa:2048 -keyout private_key.pem -out cert.pem -sha256 -days 365 -nodes -addext "subjectAltName=URI:urn:<IOC>@<HOST>:EPICS:IOC,IP:<IP>"
```
Depending on you openssl configuration the above command may not set all the required attributes and may also set `CA:TRUE`.
You can display the contents of a certificate using
```bash
openssl x509 -in cert.pem -noout -text
```
and check the `X509v3 extensions` section.

To control setting attributes you can create a `opcua_cert.conf` configuration file as follows after editing the `< >` parts of the `[ dn ]` section
```ini
[ req ]
default_bits = 2048
encrypt_key = no
distinguished_name = dn
x509_extensions = ext_x509
default_md = sha256
prompt = no

[ dn ]
CN = ${ENV::HOST}
emailAddress = <email address>
O = <Organisation>
OU = <Organisation Unit>
ST = <State/Province/County>
C = <Country abbreviation>

[ ext_x509 ]
basicConstraints = critical, CA:FALSE
keyUsage = critical, digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment, keyCertSign
extendedKeyUsage = critical, serverAuth, clientAuth
subjectAltName = URI:urn:${ENV::IOC}@${ENV::HOST}:EPICS:IOC,IP:${ENV::IP}
```
You can then use this file with the openssl command
```bash
env IP=<IP> HOST=<HOST> IOC=<IOC> openssl req -x509 -config opcua_cert.conf -newkey rsa:2048 -keyout client_private_key.pem -out client_certificate.pem -days 365
```
where `<IP>`, `<HOST>` and `<IOC>` have the same meaning as described earlier
