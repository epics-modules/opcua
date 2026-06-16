# Debugging and Tracing

The OPC UA Device Support module provides
several tools for debugging and tracing.

## IOC Shell Commands

Several commands are available for inspecting the state of the module:

*   `opcuaShow`:
    Prints information about
    sessions, subscriptions, items, and data elements.
    Supports glob patterns.
*   `opcuaShowSecurity`:
    Prints information about the IOCś security setup
    and discovered endpoints on the remote server.
*   `opcuaConnect` / `opcuaDisconnect`:
    Manually manage session connections.

Example:
```
opcuaShow OPC* 1
```

## Session/Subscription Debugging

Sessions and Subscriptions have a `debug` option.
Setting it to `>= 1` (verbosity level)
will enable printing debug information
when the client handles things related to it.

Example:
```
opcuaOption OPC1 debug=5
```

This option can be changed at runtime.
To see debug messages from initial connection handling,
enable it before `iocInit`.

## EPICS Record Debugging

You can enable tracing and debugging for specific EPICS records
by setting the `TPRO` field to a value `>= 1`.

*   In `.db` file:
    `field(TPRO, "1")`
*   On the IOC:
    `dbpf <RECORD_NAME>.TPRO 1`
*   In a shell:
    `caput <RECORD_NAME>.TPRO 1`

## Adaptive Concurrency Monitoring

:::{versionadded} 0.12
Adaptive concurrency automatically throttles requests
based on server response times.
This allows the IOC to gracefully handle slow and fast servers,
adapting automatically to the available bandwidth.

If adaptive concurrency is enabled,
you can monitor its state using `opcuaShow`.
It will display current limits and performance metrics.
:::

## Using UaExpert (External Tool)

UaExpert is a powerful free graphical OPC UA client tool
from Unified Automation (it uses the same UA SDK)
that can be used to:
1.  Test connection to the OPC UA server.
2.  Browse the address space.
3.  Identify `NamespaceIndex` and `Identifier` (NodeID)
    for EPICS record configuration.
