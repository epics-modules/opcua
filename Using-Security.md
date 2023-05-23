# Using OPC UA Security

## Concepts

The security features of OPC UA are based on the widely used X.509 certificates and the concepts of *Public Key Infrastructure* (PKI).

Certificates are filed in a *Certificate Store*, containing trusted and own certificates as well as certificates from certificate authorities (CAs). On Linux, a file based store is used, defined by four specific locations for certificates and revocation lists.

In the basic, explicit form, trusting a certificate usually means that the certificate (file) is moved into a specific folder in the certificate store.

When using certificate authorities (CAs), a certificate is trusted if it is signed by a trusted CA. This approach is preferable for a larger installation, as it does not require updating all servers and clients when new peers with new certificates are added.

A good analogy to certificates are passports: In a world with only a handful people, fabricating your passport yourself is reasonable. In reality, with millions of passport holders, it helps a lot that passports are fabricated by government agencies and this fact can be verified by checking holograms, watermarks etc.

### Self-signed Certificates

Servers often come with preloaded self-signed server certificates and a setting that will generously trust any valid self-signed client certificate. Generic clients typically offer an easy way to create a self-signed client certificate.

These settings are intended to get users started with security easily and create minimal frustration during development. These "simple" self-signed certificates are good enough for securing the communication, but they are not very practical for authentication: every self-signed certificate must be trusted explicitly - or be accepted by default.

### Communication Security

Each OPC UA application (server and client) needs an *Application Instance Certificate* and a related public/private key pair to identify itself to its communication peers. The public key is contained in the certificate; the private key is secret and used for signing and encryption of messages.

For a secure communication to be established, both peers must trust the certificate of the other.

OPC UA defines three *Message Security Modes*:

| Message Security Mode |                                             |
| --------------------- | ------------------------------------------- |
| None                  | No security is applied.                     |
| Sign                  | All messages are signed, but not encrypted. |
| SignAndEncrypt        | All messages are signed and encrypted.      |

As security algorithms are getting obsolete, an increasing number of *Security Policies* (sets of security algorithms and key lengths) are defined, by their unique URIs:

| Security Policy            | URI                                                          |
| -------------------------- | ------------------------------------------------------------ |
| None                       | http://opcfoundation.org/UA/SecurityPolicy#None              |
| Basic128Rsa15 *(obsolete)* | http://opcfoundation.org/UA/SecurityPolicy#Basic128Rsa15     |
| Basic256                   | http://opcfoundation.org/UA/SecurityPolicy#Basic256          |
| Basic256Sha256             | http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256    |
| Aes128_Sha256_RsaOaep      | http://opcfoundation.org/UA/SecurityPolicy#Aes128_Sha256_RsaOaep |
| Aes256_Sha256_RsaPss       | http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss |

### Client Authentication (Identity)

For client authentication related to managing the authorization of access to a server, OPC UA defines four *User Token Types* (methods of user authentication):

| User Token Type          |                                            |
| ------------------------ | ------------------------------------------ |
| Anonymous Identity Token | No user information available.             |
| Username Identity Token  | User identified by name and password.      |
| X.509 Identity Token     | User identified by an X.509v3 certificate. |
| Issued Identity Token    | User identified by a WS-SecurityToken.     |

### Endpoint Discovery

In order to establish a secure connection, the client has to find out which security features the server provides. This process is called *Discovery*.

In its answer to a discovery request, a server provides a list of *Endpoints* for the client to connect to, with different sets of security features.

As a debugging tool, the iocShell command `opcuaShowSecurity` runs the discovery service and displays the available endpoints with their features and their *security level* (used by the IOC to find the endpoint providing the best security).

## IOC Configuration

An IOC using the OPC UA Device Support is considered to be a production system. As such, it has to be fully configured and supplied with the certificates needed to connect to the configured OPC UA servers. Other than general purpose and example clients, the IOC cannot and should not interactively create self-signed certificates or change the trust for a certificate that a server presents.

To support setting up the certificate store, the `opcuaSaveRejected` iocShell command will configure the IOC to save rejected (untrusted) server certificates in a specified location (default: `/tmp/<ioc>@<host>`), using DER format. Copying such a certificate file to the IOC's certificate store (under `trusted/certs`) will explicitly trust the certificate and allow secure connections to the server.

When using the UA SDK low level client, the trusted server and CA certificates (under `trusted/certs`) must use DER format. ([See below](#managing-certificates) for how to convert between formats.)

Alternatively, you can use a general purpose client (e.g., the `UaExpert` tool) to connect to the server, trust its certificate, and use the certificate file from that client's certificate store. (You could also import it into your certificate management tool, [see below]((#managing-certificates)).)

*Important Note:* During the process of endpoint discovery and saving the untrusted server certificate, a man-in-the-middle could send its own certificate to the IOC. Only add a server certificate to the trusted certificate store if you are sure it actually originates from the server (e.g., on a trusted network, after verifying its thumb print). This act of trusting is the crucial and weakest point when using self-signed certificates.

### Certificate Store Setup

The iocShell command `opcuaSetupPKI` sets the location(s) for the certificate store. The IOC needs to have read access to all files in the PKI store, further access should not be granted as this might compromise security.

In its simple form, the single argument defines the location of the PKI certificate store, using default subdirectories for trusted peer and CA certificates (`trusted/certs`), CA certificate revocation lists (`trusted/crl`), intermediate issuer certificates (`issuers/certs`) and their certificate revocation lists (`issuers/crl`).

In the fully detailed form (using four arguments), the four locations are specified separately.

### Client Certificate

The iocShell command `opcuaClientCertificate` sets the locations for the client certificate (PEM or DER format) and the matching private key (PEM format).

### Session Security Setting

Two security-related session options are used to configure the security features for a given OPC UA session, by calling `opcuaSetOption` in the iocShell.

Setting `sec-mode` selects the specified message security mode for the connection. The special keyword "best" (default) will have the IOC choose the best mode, based on the server-supplied *security level*. Setting `sec-policy` (to the short name, not the full URI) selects a specific policy. If multiple endpoints match the option settings, the IOC will always choose the best available security.

*Note:* To connect without security, you have to explicitly set `sec-mode` to `None`.

If no matching endpoint is discovered or the server certificate is untrusted, the IOC will not connect.

### Identity (Client Authentication)

Without configuration, an Anonymous Identity Token will be used.

To use a Username Identity or a Certificate Identity Token, prepare an identity file. This file should only be readable by the IOC. Configure the filename through the session option `sec-id`.

In the identity file, lines starting with `#` are considered comments and ignored.

To use a Username Identity Token, set `user=<username>` and `pass=<password>`, each on a separate line.

To use a Certificate Identity Token, set `cert=<certificate file>` and `key=<private key file>`, each on a separate line, to identify the DER or PEM certificate and PEM private key to use. If the private key is password protected, add another line setting `pass=<password>`. Normally, the server will use the Common Name (CN) property of the certificate to determine the user name for authorization.

## Managing Certificates

The underlying OpenSSL library provides a command line utility.

For managing a larger number of certificates, [Xca](https://hohnstaedt.de/xca/) is a powerful and popular GUI for X.509 certificate and key management, including certificate authority (CA) functionality, which is the most efficient way to manage the certificates for a larger installation.

The `openssl` command line utility can be used to convert certificates (and keys) between formats. To convert certificate `<cert>`  from DER to PEM format, use:

```bash
openssl x509 -inform der -in <cert>.der -out <cert>.pem
```

To convert in the other direction, use:

```bash
openssl x509 -in <cert>.pem -outform der -out <cert>.der
```

The UA SDK client and the underlying OpenSSL library are picky in terms of file name extensions. DER format requires certificates to carry a `.der` extension and revocation lists to be named `.crl`, while PEM format works only if files have a `.pem` extension. Otherwise you may experience unspecific `Bad` status errors.

### Creating Application Instance Certificates

Creating a self-signed certificate for OPC UA use is pretty straight-forward. Follow the documented procedure, giving your certificate/key pair the following properties:

-   Choose the issuer information to match your situation.

-   Sign using `SHA 256` (i.e., `sha256WithRSAEncryption`).

-   X509v3 Basic Constraints: **critical**, `CA:FALSE`.

-   X509v3 Key Usage: **critical**, `Digital Signature`, `Non Repudiation`, `Key Encipherment`, `Data Encipherment`, `Certificate Sign`.

-   X509v3 Extended Key Usage: **critical**, `TLS Web Server Authentication`, `TLS Web Client Authentication`.

-   X509v3 Subject Alternative Name: `URI:urn:<ioc>@<host>:EPICS:IOC`, `DNS:<host>`
    with `<ioc>` being the IOC name,`<host>` being the hostname (i.e., the result of a `gethostname()` call) of the machine that runs the IOC. The URI tag *must* match what the Device Support module sets as its application URI.
    Alternatively, a numerical `IP Address` tag can be used instead of `DNS`.

The source tree contains an Xca certificate template that can be imported and modified to fit your needs, which will simplify generating new IOC client certificates. 

For the IOC, save the certificate in DER or PEM format, the private key as PEM. The server may need different formats - refer to the documentation of your server for more details.

A simple client certificate/key pair can also be created using the `openssl` command line utility, e.g.:

```bash
openssl req -x509 -newkey rsa:2048 -keyout private_key.pem -out cert.pem -sha256 -days 365 -nodes -addext "subjectAltName=URI:urn:<IOC>@<HOST>:EPICS:IOC,IP:<IP>"
```

### Certificates and Network / DNS Setup

The `URI:`, `DNS:` and `IP:` entries in the Subject Alternative Name section require the network and DNS to be set up correctly, otherwise the certificates will not work.

This applies to both the IOC and the server. If any of the two uses a host name in its certificate that doesn't match the host name that the other side gets when doing a reverse lookup, the connection is likely to fail.

The IOC uses the `gethostname()` result in the `URI:` entry, which might differ from its DNS host name that has to appear in the `DNS:` entry. Depending on your DNS setup, the host names in the `DNS:` entry need to be simple or fully qualified. Finding out the right way to set up your certificates may be frustrating and time consuming.

### Creating Identity Token Certificates

Create a self-signed certificate, following the documented procedure and giving your certificate/key pair the following properties:

-   Choose the issuer information to match your situation.
    The Common Name (CN) property of the certificate will normally be used by the server to determine the user name for authorization. 

-   Sign using `SHA 256` (i.e., `sha256WithRSAEncryption`).

-   X509v3 Basic Constraints: **critical**, `CA:FALSE`.

*Note:* The Unified Automation example server accepts any trusted certificate, without further verification and without using certificate properties for authorization.

### Using a Certificate Authority

A complete discussion of this concept and the design and implementation of an appropriate public key infrastructure for a specific installation is outside the scope of this README. The [Xca online documentation](https://hohnstaedt.de/xca/index.php/documentation/stepbystep) has more details and step-by-step guides.

The basic steps are:

-   Create a Certificate Authority.
    In the simplest form this means creating a Root CA certificate/key that is configured to sign other certificates.
    The CA certificate needs to be placed in DER format inside the `trusted/certs` directory of the IOC's PKI store.

-   Create a Certificate Revocation List.
    Even if you do not plan to use the Certificate Revocation List (CRL) mechanism to deal with invalidation of certificates, you need to create an empty CRL for your CA certificate and place it in DER format inside the `trusted/crl` directory of the IOC's PKI store. Otherwise your CA certificate will not work.

-   Create Application Instance and Identity Token Certificates signed by your CA.
    When creating new certificates for your servers and clients, instead of selecting "Create a self-signed certificate" under the "Source" tab, choose a different certificate (your CA certificate) that will be used to sign the newly created certificate.

Using Xca templates can simplify certificate creation and greatly improve consistency.
