# EPICS Records for Single Scalar Variables

This guide shows how to configure EPICS records
for accessing individual single scalar variables on the OPC UA server.

## General INP/OUT Link Field Format

`@<session_name> ns=<namespace_index>;<identifier_type>=<identifier>
[<option>=<value>...]`

* `<session_name>`:
  Name of the OPC UA Session.
* `<namespace_index>`:
  Numerical namespace index.
* `<identifier_type>`:
  `s` for string identifier, `i` for numerical identifier.
* `<identifier>`:
  The string or numerical ID of the OPC UA Node.

  :::{note}
  **Escape quotation marks** (`\"`)
  if the identifier string contains them
  (e.g., `"dataBlock"."myItem"` becomes `\"dataBlock\".\"myItem\"`).
  :::

## Identifying Nodes by BrowseName

Some servers use stable BrowseNames but numerical NodeIds
that may change when the server is recreated or reconfigured.
For these, a node can be identified through a *browse path*,
i.e., the sequence of BrowseNames that leads to the node
along hierarchical references (`Organizes`, `HasComponent`, `HasProperty`, ...).

`@<session_name> [ns=<namespace_index>[;<identifier_type>=<identifier>]]
bn=<browse_path> [<option>=<value>...]`

* `<browse_path>`:
  BrowseNames separated by `/` (a leading `/` is optional).
  Each BrowseName may have a `<namespace_index>:` prefix;
  without a prefix, the `ns` option's namespace index is used.
  The ampersand (`&`) escapes the following character,
  e.g., use `&/` for a `/` that is part of a BrowseName.
  Namespace indices are mapped like the `ns` option
  if a namespace mapping is configured for the session.
* Starting node:
  The node configured through `ns=...;s=...` or `ns=...;i=...`,
  or the `Objects` folder if no identifier is configured.

For BrowseNames that are long or contain spaces or other characters
that are hard to put in a link,
the browse path can be set through the info item `opcua:BROWSENAME`
instead of the `bn` link option:

```text
record(ai, "TEMP") {
    field(DTYP, "OPCUA")
    field(INP,  "@SUB ns=3")
    info(opcua:BROWSENAME, "PLC/Data Block 1/Temperature")
}
```

The browse paths are resolved using the TranslateBrowsePathsToNodeIds service
every time the session (re)connects.
BrowseNames are not guaranteed to be unique,
so the browse path must resolve to exactly one node.
If it resolves to no node or to more than one node,
an error is printed,
the node is left unconnected,
and its records are set to INVALID (`COMM` alarm).

Examples:

```text
field(INP, "@SUB ns=3 bn=PLC/Temperature")         # from Objects folder
field(INP, "@SUB bn=/3:PLC/3:Temperature")          # explicit namespace indices
field(INP, "@SUB ns=3;s=PLC bn=Temperature")        # from node ns=3;s=PLC
```

## Available Options

:::{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `sampling`
  - `-1`
  - Sampling interval in ms [double; -1 = use publishing interval]
* - `qsize`
  - `1`
  - Server-side queue size [int; 1 = no queueing]
* - `cqsize`
  - `1.5 * qsize`
  - Client-side queue size [int; minimum 3]
* - `discard`
  - `old`
  - Discard policy on queue overrun:
    [`old`/`new` (drop oldest/drop newest)]
* - `bn`
  -
  - Browse path identifying the node (see above)
* - `register`
  - `n`
  - Register item with server for performance [`y`/`n`]
* - `deadband`
  - `0.0`
  - Deadband filter for subscriptions [double; 0.0 = no deadband]
* - `bini`
  - `read`
  - Behavior at init: `read`, `ignore`, `write`
* - `monitor`
  - `y`
  - Enable monitoring. For outputs, enables readback
* - `timestamp`
  - `server`
  - Source of timestamp: `server`/`source`
:::

:::{note}
To allow the Device Support to set the timestamp of a record,
the record's `TSE` field must be set to `-2`.
:::

## Example: Output Records with Monitor (Bidirectional Mode)

These are output records (`bo`, `ao`, `longout`, `mbbo`,...)
that send data to a single scalar variable (Item) on the OPC UA server
and also monitor it,
updating the record if the value changes.
They require an OPC UA Subscription.

```
# Boolean Output
record(bo, "$(P)FOO:BOOL") {
    field(DTYP, "OPCUA")
    field(OUT, "@SLOW ns=3;s=\"FOO\".\"BOOL\"")
    field(ZNAM, "OFF")
    field(ONAM, "ON")
}

# Analog Output
record(ao, "$(P)FOO:REALA") {
    field(DTYP, "OPCUA")
    field(OUT, "@SLOW ns=3;s=\"FOO\".\"REALA\"")
    field(DRVH, "100.0")
    field(DRVL, "0.0")
    field(HOPR, "90.0")
    field(LOPR, "10.0")
}
```

* `@SLOW`:
  The name of the Subscription to which the Item belongs.
  The Session is implicitly defined by the Subscription.
* `monitor=y` is the default for all records
  expecting updates from the OPC UA server, so it is often omitted.
* The `SCAN` should be `Passive`,
  as usual with output records,
  which is the default, so it is often omitted.

## Example: Unmonitored Output Records

These are output records (`bo`, `ao`, `longout`, `mbbo`,...)
that send data to a single scalar variable (Item) on the OPC UA server,
but don't require monitoring of the remote value.

```
# Boolean Output (unmonitored)
record(bo, "$(P)BAR:BOOL") {
    field(DTYP, "OPCUA")
    field(OUT, "@PLC0 ns=3;s=\"BAR\".\"BOOL\" monitor=n")
    field(ZNAM, "OFF")
    field(ONAM, "ON")
}

# Analog Output (unmonitored)
record(ao, "$(P)BAR:REALA") {
    field(DTYP, "OPCUA")
    field(OUT, "@PLC0 ns=3;s=\"BAR\".\"REALA\" monitor=n")
    field(DRVH, "100.0")
    field(DRVL, "0.0")
    field(HOPR, "90.0")
    field(LOPR, "10.0")
}
```

* `@PLC0`:
  The name of the Session to which the Item belongs.
  Without monitoring, there is no Subscription necessary.
* `monitor=n`:
  Explicitly disables monitoring for these unmonitored variables.

## Example: Input Records with Monitor

These are input records (`bi`, `ai`, `longin`, `mbbi`,...)
pointing to a single scalar variable (Item) on the OPC UA server
that get updated when the value changes.
They require an OPC UA Subscription and should be `I/O Intr` scanned.

```
# Binary Input
record(bi, "$(P)BAZ:BOOL") {
    field(DTYP, "OPCUA")
    field(INP, "@SLOW ns=3;s=\"BAZ\".\"BOOL\"")
    field(SCAN, "I/O Intr")
    field(ZNAM, "OFF")
    field(ONAM, "ON")
}

# Analog Input
record(ai, "$(P)BAZ:REALA") {
    field(DTYP, "OPCUA")
    field(INP, "@SLOW ns=3;s=\"BAZ\".\"REALA\"")
    field(SCAN, "I/O Intr")
    field(HOPR, "90.0")
    field(LOPR, "10.0")
}
```

* `@SLOW`:
  The name of the Subscription to which the Item belongs.
  The Session is implicitly defined by the Subscription.
* `monitor=y` is the default for all records
  expecting updates from the OPC UA server, so it is often omitted.
* The `SCAN` should be set to `I/O Intr`
  as usual for input records
  that process on data changes received from the device.

## Example: Unmonitored Input Records

These are input records (`bi`, `ai`, `longin`, `mbbi`,...)
pointing to a single scalar variable (Item) on the OPC UA server,
but don't require monitoring of the remote value.

```
# Binary Input (unmonitored)
record(bi, "$(P)QUX:BOOL") {
    field(DTYP, "OPCUA")
    field(INP, "@PLC0 ns=3;s=\"QUX\".\"BOOL\" monitor=n")
    field(ZNAM, "OFF")
    field(ONAM, "ON")
}

# Analog Input (unmonitored)
record(ai, "$(P)QUX:REALA") {
    field(DTYP, "OPCUA")
    field(INP, "@PLC0 ns=3;s=\"QUX\".\"REALA\" monitor=n")
    field(SCAN, "I/O Intr")
    field(HOPR, "90.0")
    field(LOPR, "10.0")
}
```

* `@PLC0`:
  The name of the Session to which the Item belongs.
  Without monitoring, there is no Subscription necessary.
* `monitor=n`:
  Explicitly disables monitoring for these unmonitored variables.

Without a subscription,
these records will only update (doing an OPC UA read operation)
when they are processed.

Setting a periodic `SCAN` rate is inefficient and should be avoided;
use monitored Items on subscriptions instead.

A valid use case for this type of connection are static status variables,
which do not change during a session
(e.g., server information: name, vendor, version, start date/time, ...).
They only get updated once after the connection is established.
