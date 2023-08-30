(iocsh-command-reference)=
# iocShell Command Reference

```{versionadded} 0.9
The unified `opcuaSession`, `opcuaSubscription`, and `opcuaOptions` commands
are replacing the older `opcuaCreate...` commands.
```

## Session Management

(iocsh-opcuasession)=
### Command `opcuaSession`
Configures a new OPC UA session.
```
opcuaSession <name> <serverURL> [<options>]
```
*   `name`:
    Unique name for the session.
*   `serverURL`:
    Server endpoint (e.g., `opc.tcp://localhost:4840`).
*   `options`:
    Key-value pairs (e.g., `autoconnect=n` or `debug=1`).

### Commands `opcuaConnect` / `opcuaDisconnect`
Manually connect or disconnect sessions matching a glob pattern.
```
opcuaConnect <pattern>
opcuaDisconnect <pattern>
```
*   `pattern`:
    Session name glob pattern.

For sessions using the option `autoconnect=y`,
these commands will also switch the feature accordingly.

### Command `opcuaMapNamespace`
```{versionadded} 0.8
Maps a numerical namespace index used in the database to a URI on the server.
```
```
opcuaMapNamespace <session> <index> <URI>
```
*   `session`:
    Name of the session.
*   `index`:
    Namespace index locally used.
*   `URI`:
    URI in the server's namespace table to map `index` to.

On the wire, namespaces are always integers.
A few are reserved well-known namespaces for OPC UA or the server itself.
The other namespaces are assigned by the server.
Every namespace has a unique URI, which the server adds to a "namespace array".
The number of a namespace (used on the wire)
is the index of its URI in the server's namespace array.

If a server assigns numbers dynamically, the `opcuaMapNamespace` command
allows to map a namespace number used in the IOC's databases
to its URI on the server.

````{hint}
In your session `OPC1`,
you want to connect to a device
that is dynamically mapped in the OPC UA server.
You know that the namespace URI is "urn:MyCompany:UaServer:node3".

To connect, use an arbitrary namespace index in your databases, e.g. `42`.
Then call
```
opcuaMapNamespace OPC1 42 urn:MyCompany:UaServer:node3
```
The IOC will download the server's namespace array
and replace namespace index `42` in your records
with the correct index for the specified URI.
````

## Subscription Management

(iocsh-opcuasubscription)=
### Command `opcuaSubscription`
Configures a new subscription and binds it to an existing session.
```
opcuaSubscription <name> <session> <publishing_interval> [<options>]
```
*   `name`:
    Unique name for the subscription.
*   `session`:
    Name of the session that this subscription is added to.
*   `publishing_interval`:
    Publishing interval for the subscription [ms].
*   `options`:
    Key-value pairs (e.g., `priority=50` or `debug=1`).

## Setting Options

### Command `opcuaOptions`
Sets options for existing sessions or subscriptions matching a pattern.
```
opcuaOptions <pattern> [<options>]
```
*   `pattern`:
    Session or Subscription name glob pattern.
*   `options`:
    Key-value pairs (e.g., `autoconnect=n` or `debug=1`).

### Table of Session Options

```{list-table}
:header-rows: 1

* - Name
  - Function
* - *General*
  -
* - `debug`
  - Verbosity level of debugging [default: `0` = off]
* - `autoconnect`
  - Automatically connect/reconnect to server [`y`/`n`; default: `y`]
* - *Batch and Throttle*
  -
* - `nodes-max`
  - Maximum number of nodes used in any low-level service call<br>
    (client will split larger requests into multiple batches)
* - `read-nodes-max`
  - Maximum number of nodes per read service call<br>
    (client will split larger read requests into multiple batches)
* - `read-timeout-min`
  - Timeout (holdoff period) after read service call [ms]<br>
    (used for minimal one node request if read-timeout-max is set)
* - `read-timeout-max`
  - Timeout (holdoff period) after read service call<br>
    with maximum number of nodes [ms]
* - `write-nodes-max`
  - Maximum number of nodes per write service call<br>
    (client will split larger write requests into multiple batches)
* - `write-timeout-min`
  - Timeout (holdoff period) after write service call [ms]<br>
    (used for minimal one node request if write-timeout-max is set)
* - `write-timeout-max`
  - Timeout (holdoff period) after write service call<br>
    with maximum number of nodes [ms]
* - *Security*
  -
* - `sec-mode`
  - Requested security mode [`Best`/`None`/`Sign`/`SignAndEncrypt`;<br>
    default: `Best`; must use `None` for running with no security]
* - `sec-policy`
  - Requested security policy [default: use best available]
* - `sec-id`
  - Set file to read identity credentials from
```

### Table of Subscription Options

```{list-table}
:header-rows: 1

* - Name
  - Function
* - `debug`
  - Verbosity level of debugging [default: 0 = off]
* - `priority`
  - Priority of the subscription [0..255; default: 0 = lowest]
```

## OPC UA Security Management

### Command `opcuaClientCertificate`
Sets the client's own certificate and private key.
```
opcuaClientCertificate <public_key> <private_key>
```
*   `public_key`:
    Path to the file containing the certificate (public key).
*   `private_key`:
    Path to the file containing the private key.

### Command `opcuaSetupPKI`
Sets the location of the PKI certificate store,
where certificates and revocation lists are stored.
```
opcuaSetupPKI <PKI_location>
opcuaSetupPKI <server_certs> <server_revocation_lists> <issuer_certs> <issuer_revocation_lists>
```
*   `PKI location`:
    Path to the standard PKI directory structure.
*   `server_certs`:
    Path to the location of trusted server certificates.
*   `server_revocation_lists`:
    Path to the location of server revocation lists.
*   `issuer_certs`:
    Path to the location of trusted issuer certificates.
*   `issuer_revocation_lists`:
    Path to the location of issuer revocation lists.
    
### Command `opcuaSaveRejected`
Sets the location where rejected server certificates are saved.
```
opcuaSaveRejected <rejected_cert_location>
```
*   `rejected_cert_location`:
    Path to the location for saving rejected server certificates.

### Command `opcuaShowSecurity`
Shows discovered endpoints and security details for a session
or (without argument) show security related information for the IOC.
```
opcuaShowSecurity <session>
opcuaShowSecurity
```
*   `session`:
    Name of the session to show security info for.

## Inspection and Debugging

### Command `opcuaShow`
Shows details for sessions, subscriptions, or records.
```
opcuaShow <pattern>, [<verbosity>]
```
*   `pattern`:
    Session or Subscription name glob pattern.
*   `verbosity`:
    Sets amount of printed information.
