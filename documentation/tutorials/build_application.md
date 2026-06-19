(building-an-ioc)=
# Building an IOC Application

This section provides instructions
how to build and configure an EPICS IOC
to communicate with an OPC UA server.

It also explains how to use the example application
that is part of the Device Support module.

## Use the Example Application

The Device support module contains an example application
that builds an IOC binary and creates two IOC instances,
for connecting to a Siemens S7-1500 series PLC and
for connecting to the Unified Automation C++ demo server.

### Structure of the Example Application

Under `.../exampleTop`, you will find

- `templateDbSup` - contains useful template databases
- `opcuaIocApp` - builds the OPC UA IOC binary
- `DeviceDbApp` - creates the databases for the two IOCs
- `iocBoot` - contains the startup directories for the two IOCs

The IOC for the demo server provides a number of records
that connect to variables in the demo server.
It shows scalars of different types,
arrays of scalars,
"dynamic" variables (updated by the demo server),
"static" variables (can be read and written by the IOC)
and the "WorkOrder" as an example for structured data.

The IOC for the S7-1500 contains some example records
for an imaginary "DB1" on the PLC.
As the names are completely determined by the application code on the PLC,
the example application cannot show more details.

Both IOCs contain "server" records.
These are connecting to server variables in namespace 0,
which are part of the OPC UA specification and mandatory:
every OPC UA server must provide them.

### Start Up Command File `st.cmd`

Sessions and subscriptions are set up
in your IOC's startup script (e.g., `st.cmd`).

See also: [iocShell Command Reference](#iocsh-command-reference)

Looking at the `st.cmd` of the Demo Server IOC...

```
opcuaSession OPC1 opc.tcp://localhost:48010 sec-mode=None
opcuaSubscription SUB1 OPC1 200
```

This creates a Session named `OPC1` (without security)
connecting to the Demo Server running on the same host.
A single subscription `SUB1` is added to that session,
with a publishing interval of 200ms.

:::{note}
To connect without security,
you have to explicitly set the option `sec-mode=None`
for the session.
:::

```
opcuaMapNamespace OPC1 2 "http://www.unifiedautomation.com/DemoServer/"
```

This maps the namespace `ns=2` for links on this Session
to whatever number the server uses
for the namespace identified by the specified URL.

(This is necessary, because starting from some version
Unified Automation changed the namespace used by the Demo Server.
Using the mapping ensures that `ns=2` always works.)

In the databases for the Demo Server IOC,
you find records like the following examples.

### Input Records

```
record(longin, "$(P)$(R)liuint16") {
    field(DTYP, "OPCUA")
    field(INP,  "@$(SUBS) ns=2;s=Demo.Dynamic.Scalar.UInt16")
    field(SCAN, "I/O Intr")
    field(TSE,  "-2")
    field(TPRO, "$(DEBUG=0)")
}
```

Which is an `longin` input record,
monitoring an OPC UA variable of type UInt16
from the "Dynamic" set that gets updated by the server code. 

* `$(P)$(R)liuint16`:
  The record name.
  `$(P)` and `$(R)` are prefixes defined in your `st.cmd`.
* `DTYP, "OPCUA"`:
  Specifies that this record uses the OPC UA Device Support.
* `INP, "@$(SUBS) ns=2;s=Demo.Dynamic.Scalar.UInt16"`:
  This is the crucial link field.
  - `@$(SUBS)`:
    The name of the OPC UA subscription to use,
    set through a macro.
  - `ns=2`:
    The namespace index of the OPC UA item.
  - `s=Demo.Dynamic.Scalar.UInt16`:
    The string identifier of the OPC UA item.
* `SCAN, "I/O Intr"`:
  Configures the record to process upon reception
  of new data from the OPC UA server,
  which is efficient for subscriptions.
* `TSE, "-2"`:
  Use OPC UA timestamps as EPICS record timestamp.
* `TPRO, "$(DEBUG=0)`:
  Set debug level through a macro.

### Output Records (Bidirectional)

```
record(aao, "$(P)$(R)aaobool") {
    field(DTYP, "OPCUA")
    field(OUT,  "@$(SUBS) ns=2;s=Demo.Static.Arrays.Boolean")
    field(FTVL, "UCHAR")
    field(NELM, "10")
    field(TSE,  "-2")
    field(TPRO, "$(DEBUG=0)")
}
```

Which is an array output record of type `UCHAR`,
linked to an OPC UA variable that is an Array of Boolean
from the "Static" set that gets you can read and write. 

* `$(P)$(R)aaobool`:
  The record name.
  `$(P)` and `$(R)` are prefixes defined in your `st.cmd`.
* `DTYP, "OPCUA"`:
  Specifies that this record uses the OPC UA Device Support.
* `OUT, "@$(SUBS) ns=2;s=Demo.Static.Arrays.Boolean"`:
  This is the crucial link field.
  - `@$(SUBS)`:
    The name of the OPC UA subscription to use,
    set through a macro.
  - `ns=2`:
    The namespace index of the OPC UA item.
  - `s=Demo.Static.Arrays.Boolean`:
    The string identifier of the OPC UA item.
* `FTVL, "UCHAR"`:
  Type of the EPICS array.
* `NELM, "10"`:
  Capacity of the EPICS array.
* `TSE, "-2"`:
  Use OPC UA timestamps as EPICS record timestamp.
* `TPRO, "$(DEBUG=0)`:
  Set debug level through a macro.

### Records Linking to Structured Data

```
record(opcuaItem, "$(P)$(R)itemRBK") {
    field(DTYP, "OPCUA")
    field(INP,  "@$(SUBS) ns=2;s=$(WO_NODEID)")
    field(TSE,  "-2")
}

record(stringin, "$(P)$(R)assetidRBK") {
    field(DTYP, "OPCUA")
    field(INP,  "@$(P)$(R)itemRBK element=AssetID")
    field(TSE,  "-2")
}

record(stringin, "$(P)$(R)starttimeRBK") {
    field(DTYP, "OPCUA")
    field(INP,  "@$(P)$(R)itemRBK element=StartTime")
    field(SCAN, "I/O Intr")
    field(TSE,  "-2")
}
```

An `opcuaItem` record representing the Item (structure).
It is configured "normally",
addressing the OPC UA Item (like the earlier examples),
with the string identifier supplied through a macro.

Two `stringin` records are pointing to structure elements.
In their `INP` links,
`@$(P)$(R)itemRBK` is pointing to the `opcuaItem` record
that represents the structured item,
and the `element=AssetID` and `element=StartTime` parts
link them to specific elements. 

## Create an IOC for OPC UA from Scratch

1. **Create a new EPICS application:**
   ```bash
   cd <your_epics_area>
   /path/to/epics/base/bin/<arch>/makeBaseApp.pl -t ioc MyExampleIOC
   /path/to/epics/base/bin/<arch>/makeBaseApp.pl -i -t ioc MyExampleIOC
   ```

2. **Add the OPC UA Device Support:**
   Add an entry setting `OPCUA = <path_to_the_opcua_module>`
   to your `RELEASE.local` file.
    
3. **Add `opcua` to your `Makefile`:**
   Edit `MyExampleIOCApp/src/Makefile`
   and add `opcua` to the list of libraries to link against
   and to the list of DBD files to use.
   ```makefile
   # Include dbd files from all support applications:
   MyExampleIOC_DBD += opcua.dbd
   # ...
   # Add all the support libraries needed by this IOC
   MyExampleIOC_LIBS += opcua
   ```

4. **Build the application:**
   ```bash
   make
   ```

5. **Finish your Application:**
   Add databases.
   Load the databases
   and create the necessary OPC UA Sessions/Subscriptions\
   in your `st.cmd` script.
