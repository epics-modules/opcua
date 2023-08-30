# OPC UA Security Concepts

The security features of OPC UA
are based on the widely used X.509 certificates
and the concepts of *Public Key Infrastructure* (PKI).

Certificates are filed in a *Certificate Store*,
containing trusted and own certificates
as well as certificates from certificate authorities (CAs).
On Linux, a file based store is used,
defined by four specific locations for certificates and revocation lists.

In the basic, explicit form,
trusting a certificate usually means
that the certificate (file) is moved into a specific folder
in the certificate store.

When using certificate authorities (CAs),
a certificate is trusted if it is signed by a trusted CA.
This approach is preferable for a larger installation,
as it does not require updating all servers and clients
when new peers with new certificates are added.

A good analogy to certificates are passports:
In a world with only a handful people,
fabricating your passport yourself is reasonable.
In reality, with millions of passport holders,
it helps a lot that passports are fabricated by government agencies
and this fact can be verified by checking holograms, watermarks etc.

## Self-signed Certificates

Servers often come with preloaded self-signed server certificates
and a setting that will generously trust
any valid self-signed client certificate.
Generic clients typically offer an easy way
to create a self-signed client certificate.

These settings are intended to get users started with security easily
and create minimal frustration during development.
Such "simple" self-signed certificates
are good enough for securing the communication,
but they are not very practical for authentication:
every self-signed certificate must be trusted explicitly - 
or be accepted by default.

## Communication Security

Each OPC UA application (server and client) needs an *Application Instance Certificate* and a related public/private key pair to identify itself to its communication peers. The public key is contained in the certificate; the private key is secret and used for signing and encryption of messages.

For a secure communication to be established, both peers must trust the certificate of the other.

OPC UA defines three *Message Security Modes*:

| Message Security Mode |                                             |
| --------------------- | ------------------------------------------- |
| None                  | No security is applied.                     |
| Sign                  | All messages are signed, but not encrypted. |
| SignAndEncrypt        | All messages are signed and encrypted.      |

As security algorithms are getting obsolete,
an increasing number of *Security Policies*
(sets of security algorithms and key lengths) are defined,
identified their unique URIs:

```{list-table}
:header-rows: 1

* - Security Policy
  - URI
* - None
  - http://opcfoundation.org/UA/SecurityPolicy#None
* - Basic128Rsa15 *(obsolete)*
  - http://opcfoundation.org/UA/SecurityPolicy#Basic128Rsa15
* - Basic256
  - http://opcfoundation.org/UA/SecurityPolicy#Basic256
* - Basic256Sha256
  - http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256
* - Aes128_Sha256_RsaOaep
  - http://opcfoundation.org/UA/SecurityPolicy#Aes128_Sha256_RsaOaep
* - Aes256_Sha256_RsaPss
  - http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss
```

## Client Authentication (Identity)

For client authentication
related to managing the authorization of access to a server,
OPC UA defines four *User Token Types* (methods of user authentication):

| User Token Type          |                                            |
| ------------------------ | ------------------------------------------ |
| Anonymous Identity Token | No user information available.             |
| Username Identity Token  | User identified by name and password.      |
| X.509 Identity Token     | User identified by an X.509v3 certificate. |
| Issued Identity Token    | User identified by a WS-SecurityToken.     |

## Endpoint Discovery

In order to establish a secure connection,
the client has to find out which security features the server provides.
This process is called *Discovery*.

In its answer to a discovery request,
a server provides a list of *Endpoints* for the client to connect to,
with different sets of security features.

As a debugging tool,
the iocShell command `opcuaShowSecurity` runs the discovery service
and displays the available endpoints
with their features and their *security level*
(used by the IOC to find the endpoint providing the best security).
