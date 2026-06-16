# EPICS Records for Structured Data

:::{versionadded} 0.3
The `opcuaItem` record type and support for structured data.
:::

This guide shows how to configure EPICS records
for accessing structured data on the OPC UA server.
This is achieved using the `opcuaItem` record type
in combination with regular EPICS records.

The `opcuaItem` record connects to the Item
that represents the structure.
Leaf elements are accessed through regular EPICS records,
one for each element that is interfaced.

## Creating the Root `opcuaItem`

### INP Link Field Format for the `opcuaItem` Record

This format is the same as for the INP/OUT links
of single scalar variables.

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

### Available Options

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
  - Discard policy on queue overrun: [`old`/`new` (drop oldest/drop newest)]
* - `register`
  - `n`
  - Register item with server for performance [`y`/`n`]
* - `bini`
  - `read`
  - Behavior at init: `read`, `ignore`, `write`
* - `monitor`
  - `y`
  - Enable monitoring. For outputs, enables readback
* - `timestamp`
  - `server`
  - Source of timestamp: `server`/`source`/`@<top_element>`
    [`top_element` points to source of timestamp]
:::

### `opcuaItem` Record Configuration

The `opcuaItem` record connects to a structured variable (Item)
on the OPC UA server.
It requires an OPC UA subscription.

The `opcuaItem` record is bidirectional.
Explicit `read` and `write` operations can be triggered
by writing to the (pseudo) fields `READ` or `WRITE`.

```
record(opcuaItem, "$(P)FOO:OPCUA-ITEM") {
    field(DTYP, "OPCUA")
    field(INP, "@SLOW ns=3;s=\"DB1\".\"FOO\"")
    field(SCAN, "I/O Intr")
    field(DEFACTN, "read")
    field(WOC, "immediate")
}
```
*   `@SLOW`:
    The subscription name.
*   `ns=3;s=\"DB1\".\"FOO\""`:
    Points to the structured OPC UA Item.
*   `SCAN, "I/O Intr"`:
    Ensures the record processes when the structured data changes.
*   `DEFACTN` specifies the default action
    when the record is processed: `read` or `write`.
*   `WOC` controls the writing behavior
    when single elements of the structure are changed.
    `manual` (default)
    requires explicit processing of the record to write the structure.
    `immediate`
    processes the record (writes the structure)
    as soon as an element changes.
    This can be used to avoid writing inconsistent data
    while multiple elements of the structure are written in a transactional fashion,
    e.g., when restoring a snapshot or loading a configuration.

## Linking Element Records to `opcuaItem`

Individual elements within the structured item
are accessed by other regular input/output records.

### INP/OUT Link Field Format for Elements

`@<opcuaItem_record_name> [element=<element_name>] [<option>=<value>...]`

*   `<opcuaItem_record_name>`:
    The name of the `opcuaItem` record managing the structured data.
*   `element=<element_name>`:
    The path to the specific element within the structure.
    Use `.` as a hierarchy separator.

### Available Options

:::{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `element`
  - 
  - Path to element inside an `opcuaItem` structure
    [`.` = hierarchy separator; `<empty>` = root element if no structure]
* - `bini`
  - `read`
  - Behavior at init: `read`/`ignore`/`write`
* - `monitor`
  - `y`
  - Enable monitoring. For outputs, enables readback
* - `timestamp`
  - `server`
  - Source of timestamp: `server`/`source`/`data`
:::

### Example: Input Records for Elements of `$(P)FOO:OPCUA-ITEM`

```
# Binary Input for a boolean element
record(bi, "$(P)FOO:BOOL") {
    field(DTYP, "OPCUA")
    field(INP, "@$(P)FOO:OPCUA-ITEM element=STRUCT.BOOL")
    field(SCAN, "I/O Intr")
    field(ZNAM, "OFF")
    field(ONAM, "ON")
}

# Analog Input for a real element
record(ai, "$(P)FOO:REALA") {
    field(DTYP, "OPCUA")
    field(INP, "@$(P)FOO:OPCUA-ITEM element=STRUCT.REAL")
    field(SCAN, "I/O Intr")
}
```
*   `element=STRUCT.BOOL`:
    Refers to the `BOOL` sub-element inside the `STRUCT` element
    of the structure that the `opcuaItem` record points to.
