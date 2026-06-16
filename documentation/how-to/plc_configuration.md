# Configuring the OPC UA Server on a Siemens S7-1500 Series PLC

This guide details the steps to configure a Siemens S7-1500 PLC
as an OPC UA server using TIA Portal.

## Activate OPC UA Server

1.  Open your project in TIA Portal.
2.  Navigate to your PLC's properties (e.g., `PLC_1 [CPU 1518-4 PN/DP]`).
3.  Go to `General` > `OPC UA` > `Server` > `General`.
4.  Check the `Activate OPC UA server` box.
5.  Ensure that one of the `Server addresses` (`opc.tcp://...`)
    matches the IP address of your PLC on the network.
    The default OPC UA port is 4840.

### Using OPC UA with a CP Module

OPC UA works using a CP module
when respecting the following minimal requirements:

| Feature                | Minimum Requirement |
| ---------------------- | ------------------- |
| Virtual interface (W1) | CPU FW **V2.8**     |
| Configuration in TIA   | TIA Portal **V16**  |
| OPC UA via CP          | Supported via W1    |
| Compatible CP          | CP 1543‑1 FW ≥ **V2.2** (recommended ≥ V3.0) |


## Configure OPC UA Server Options

1.  In the PLC properties,
    navigate to `General` > `OPC UA` > `Server` > `Options`.
2.  **Port:**
    Adjust the `Port` number
    if a different port than the default 4840 is required.
3.  **Minimum sampling interval:**
    Defines the minimum rate (in ms)
    at which the server can check data sources for changes.
4.  **Minimum publishing interval:**
    Defines the minimum rate (in ms)
    at which the server can send updated data to clients.

    :::{hint}
    For S7-1500 PLCs,
    especially small or mid-size ones (e.g., S7-1516),
    starting with 250ms intervals
    and grouping variables into structures
    is a good practice for improving performance.
    :::

## Configure OPC UA Server Security

1.  In the PLC properties,
    navigate to `General` > `OPC UA` > `Server` > `Security`.
2.  **Security Policies:**
    Select the desired security policy.
    For basic testing and development,
    you might select `No security`.
    For production environments,
    stronger options like `Basic128Rsa15-Sign & Encrypt` are recommended.
3.  **Guest Authentication:**
    If no specific user authentication is required,
    check `Enable guest authentication`.

    :::{attention}
    On the IOC, OPC UA Security is enabled by default.
    To connect to any OPC UA server *without* security,
    you must set the option `sec-mode=None`
    for the concerned session(s) in your EPICS IOC configuration.
    :::

## Configure OPC UA Runtime License

1.  In the PLC properties,
    navigate to `General` > `Runtime licenses` > `OPC UA`.
2.  Ensure the `Type of purchased license`
    is at least the same level as the `Type of required license`
    (e.g., `SIMATIC OPC UA S7-1500 large`).

## Define Variables in PLC Data Structures

Organize your PLC variables into data blocks
that will be exposed as OPC UA nodes.

1.  **Create Data Blocks:**
    In TIA Portal,
    create new data blocks (e.g., `DB1`, `DB2`, `DB3`, `DB4`).
2.  **Define Variables:**
    Inside each data block,
    define the variables you want to expose.
    You can use structured user-defined data types,
    which will appear as structured data on OPC UA.
    Structured data is very efficient when using subscriptions.
    For structures, the server on the S7 always creates
    a node for the structure
    and separate nodes for each of its elements.

### Example PLC Data Types and EPICS Record Type Compatibility

:::{list-table}
:header-rows: 1

* - PLC data type
  - EPICS Record type
* - Real, LReal
  - ai/ao
* - Bool
  - bi/bo
* - Byte/Word/DWord, most *Int
  - longin/longout, mbbi/mbbo, mbbiDirect/mbboDirect, bi/bo, ai/ao (RVAL)
* - LInt/ULInt
  - int64in/int64out
* - String
  - stringin/stringout, lsi/lso
* - Array[m..n] of *
  - aai/aao/waveform (with compatible FTVL)
* - Struct (UDT)
  - opcuaItem
:::

Arrays that start at a non-zero index on the PLC
are shifted to start from zero over OPC UA.

:::{note}
Structured items can *only* be handled
using the `opcuaItem` record type.
:::
