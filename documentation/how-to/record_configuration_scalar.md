# EPICS Records for Single Scalar Variables

This guide shows how to configure EPICS records
for accessing individual single scalar variables on the OPC UA server.

## General INP/OUT Link Field Format

`@<session_name> ns=<namespace_index>;<identifier_type>=<identifier>
[<option>=<value>...]`

*   `<session_name>`:
    Name of the OPC UA Session.
*   `<namespace_index>`:
    Numerical namespace index.
*   `<identifier_type>`:
    `s` for string identifier, `i` for numerical identifier.
*   `<identifier>`:
    The string or numerical ID of the OPC UA Node.

    :::{note}
    **Escape quotation marks** (`\"`)
    if the identifier string contains them
    (e.g., `"dataBlock"."myItem"` becomes `\"dataBlock\".\"myItem\"`).
    :::

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
* - `register`
  - `n`
  - Register item with server for performance [`y`/`n`]
* - `deadband`
  - 0.0
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
*   `@SLOW`:
    The name of the Subscription to which the Item belongs.
    The Session is implicitly defined by the Subscription.
*   `monitor=y` is the default for all records
    expecting updates from the OPC UA server, so it is often omitted.
*   The `SCAN` should be `Passive`,
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
*   `@PLC0`:
    The name of the Session to which the Item belongs.
    Without monitoring, there is no Subscription necessary.
*   `monitor=n`:
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
*   `@SLOW`:
    The name of the Subscription to which the Item belongs.
    The Session is implicitly defined by the Subscription.
*   `monitor=y` is the default for all records
    expecting updates from the OPC UA server, so it is often omitted.
*   The `SCAN` should be set to `I/O Intr`
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
*   `@PLC0`:
    The name of the Session to which the Item belongs.
    Without monitoring, there is no Subscription necessary.
*   `monitor=n`:
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
