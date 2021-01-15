# Using OPC UA Security

## Concepts

The security features of OPC UA are based on the widely used X.509 certificates and the concepts of *Public Key Infrastructure* (PKI).

Certificates are filed in a *Certificate Store*, containing trusted and own certificates as well as certificates from certificate authorities (CAs). On Linux, a file based store is used, defined by four specific locations for certificates and revocation lists.

In the basic, explicit form, trusting a certificate usually means that the certificate (file) is moved into a specific folder in the certificate store.

When using certificate authorities (CAs), a certificate is trusted if it is signed by a trusted CA. This approach is preferable for a larger installation, as it does not require updating all servers and clients when new peers with new certificates are added.

A good analogy to certificates are passports: In a world with only a handful people, fabricating your passport yourself is reasonable. In reality, it helps a lot that passports are fabricated by a government agency and this fact can be verified by checking holograms, watermarks etc.

### Self-signed Certificates

Servers often come with preloaded self-signed server certificates and a setting that will generously trust any valid self-signed client certificate. Generic clients typically offer an easy way to create a self-signed client certificate.

These settings are intended to get users started with security easily and create minimal frustration during development. These "simple" self-signed certificates are good enough for securing the communication, but they are not very practical for authentication: every self-signed certificate must be trusted explicitly - or be accepted by default.

### Communication Security

Each OPC UA application (server and client) needs an *Application Instance Certificate* and a related public/private key pair to identify itself to its communication peers. The public key is contained in the certificate; the private key is secret and used for signing and encryption of messages.

For a secure communication to be established, both peers must trust the certificate of the other.

OPC UA defines *Security Policies* (sets of security algorithms and key lengths) with their unique URIs:

| Security Policy            | URI                                                          |
| -------------------------- | ------------------------------------------------------------ |
| None                       | http://opcfoundation.org/UA/SecurityPolicy#None              |
| Basic128Rsa15 *(obsolete)* | http://opcfoundation.org/UA/SecurityPolicy#Basic128Rsa15     |
| Basic256                   | http://opcfoundation.org/UA/SecurityPolicy#Basic256          |
| Basic256Sha256             | http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256    |
| Aes128_Sha256_RsaOaep      | http://opcfoundation.org/UA/SecurityPolicy#Aes128_Sha256_RsaOaep |
| Aes256_Sha256_RsaPss       | http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss |

Three *Message Security Modes* are defined:

| Message Security Mode |                                             |
| --------------------- | ------------------------------------------- |
| None                  | No security is applied.                     |
| Sign                  | All messages are signed, but not encrypted. |
| SignAndEncrypt        | All messages are signed and encrypted.      |

### Client Authentication (Identity)

For authentication related to managing the authorization of access to a server, OPC UA defines four *User Token Types* (methods of user authentication):

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

When using the UA SDK low level client, the trusted server certificates (under `trusted/certs`) must use DER format. ([See below](#managing-certificates) for how to convert between formats.)

Alternatively, you can use a general purpose client (e.g., the `UaExpert` tool) to connect to the server, trust its certificate, and use the certificate file from that client's certificate store. (You could also import it into your certificate management tool, [see below]((#managing-certificates)).)

*Important Note:* During the process of endpoint discovery and saving the untrusted server certificate, a man-in-the-middle could send its own certificate to the IOC. Only add a server certificate to the trusted certificate store if you are sure it actually originates from the server (e.g., on a trusted network, after verifying its thumb print). This act of trusting is the crucial and weakest point when using self-signed certificates.

### Certificate Store Setup

The iocShell command `opcuaSetupPKI` sets the location(s) for the certificate store. The IOC needs to have read access to all files in the PKI store, further access should not be granted as this might compromise security.

In its simple form, the single argument defines the location of the PKI certificate store, using default subdirectories for peer certificates (`trusted/certs`), peer certificate revocation lists (`trusted/crl`), issuer certificates (`issuers/certs`) and issuer certificate revocation lists (`issuers/crl`).

In the fully detailed form (using four arguments), the four locations are specified separately.

### Client Certificate

The iocShell command `setClientCertificate` sets the locations for the client certificate (PEM or DER format) and the matching private key (PEM format).

### Session Security Setting

Three security-related session options can be used to configure the security features for a given OPC UA session, by calling `opcuaSetOption` in the iocShell.

Setting `sec-mode` requires the specified message security mode for the connection; setting `sec-policy` (to the short name, not the URI) requires a specific policy. Setting `sec-level` specifies the minimal security level for an endpoint to be used.

The IOC will always choose the best available security that matches all option settings. Leaving the options in their default setting (`None`/`None`/ `0`) will connect without security.

If no matching endpoint is discovered or the server certificate is untrusted, the IOC will not connect.

### Identity (Client Authentication)

Without configuration, an Anonymous Identity Token will be used.

To use a Username Identity or a Certificate Identity Token, prepare an identity file. This file should only be readable by the IOC. Configure the filename through the session option `sec-id`.

In the identity file, lines starting with `#` are considered comments and ignored.

To use a Username Identity Token, set `user=<username>` and `pass=<password>`, each on a separate line.

To use a Certificate Identity Token, set `cert=<certificate file>` and `key=<private key file>`, each on a separate line, to identify the DER or PEM certificate and PEM private key to use. If the private key is password protected, add another line setting `pass=<password>`. Normally, the server will use the Common Name (CN) property of the certificate to determine the user name for authorization.

## Managing Certificates

[Xca](https://hohnstaedt.de/xca/) is a powerful and popular GUI for managing X.509 certificates and keys, including the certificate authority (CA) functionality needed for managing the certificates for a larger installation.

The `openssl` command line utility can be used to convert certificates (and keys) between formats. To convert certificate `<cert>`  from DER to PEM format, use:

```bash
openssl x509 -inform der -in <cert>.der -out <cert>.crt
```

To convert in the other direction, use:

```bash
openssl x509 -in <cert>.crt -outform der -out <cert>.der
```

### Creating Application Instance Certificates

Creating a self-signed certificate for OPC UA use is pretty straight-forward. Follow the documented procedure, giving your certificate/key pair the following properties:

-   Choose the issuer information to match your situation.

-   Sign using `SHA 256` (i.e., `sha256WithRSAEncryption`).

-   X509v3 Basic Constraints: **critical**, `CA:FALSE`.

-   X509v3 Key Usage: **critical**, `Digital Signature`, `Non Repudiation`, `Key Encipherment`, `Data Encipherment`, `Certificate Sign`.

-   X509v3 Extended Key Usage: **critical**, `TLS Web Server Authentication`, `TLS Web Client Authentication`.

-   X509v3 Subject Alternative Name: `URI:urn:<ioc>@<host>:EPICS:IOC`, `DNS:<host>`
    with `<ioc>` being the IOC name,`<host>` being the hostname (i.e., the result of a `gethostname()` call) of the machine that runs the IOC. The URI tag *must* match what the Device Support module sets as its application URI.
    For server certificates, I have seen the URI not containing a hostname and a numerical `IP Address` tag instead of `DNS`.

The sources contain an Xca certificate template that can be imported and modified to fit your needs, which will simplify generating new IOC client certificates. 

For the IOC, save the certificate in DER or PEM format, the private key as PEM. The server may need different formats - refer to the documentation of your server for more details.

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

-   Create Application Instance and Identity Token Certificates signed by your CA.
    When creating a new certificate, under the "Source" tab, instead of selecting "Create a self-signed certificate", choose a different certificate (your Root CA) that will be used to sign the newly created certificate.

Setting up Xca templates can simplify certificate creation and greatly improve consistency.