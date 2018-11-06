# Siemens S7 1500 Series PLC

Siemens S7 1500 series PLCs provide an embedded OPC UA server
running on the PLC itself.

## Enable the OPC UA Server

Inside TIA Portal, for your PLC, inside the "Properties" / "General" tab,
under "OPC UA" / "Server", enable the OPC UA server.

Take note of the URL for your server.

Under "Runtime licenses" / "OPC UA", select the type of license
you acquired for your PLC.

_The PLC has to be disconnected for this configuration._

## Mark the items you want to access

In the list view of a Data Block, the checkboxes for
"Accessible from HMI/OPC UA" and "Writable from HMI/OPC UA"
determine which data are accessible and which are writable.

## The example databases

The S7-1500 provides all PLC data items in OPC UA namespace 3.

The example databases use a few static items created in DB1
(aka `Data_block_1`).

  Name   | Data Type |     OPC UA Node Name
-------- | --------- | -------------------------
myInt    | Int       | "Data_block_1"."myInt"
myWord   | Word      | "Data_block_1"."myWord"
myString | String    | "Data_block_1"."myString"
myBool   | Bool      | "Data_block_1"."myBool"
myDouble | Double    | "Data_block_1"."myDouble"

The double quotes are part of the OPC UA node name and have to be escaped
inside EPICS database field values.

## Use a GUI to check

When developing, using a professional GUI client for OPC UA is strongly
suggested.
The free UaExpert tool from Unified Automation is a good choice.

## OPC UA Device Support Documentation

The documentation folder of the Device Support module contains the
[Requirements Specification (SRS)](https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20SRS.pdf)
giving an introduction and the list of requirements that should convey a good
idea of the planned features.

The [Cheat Sheet](https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20Cheat%20Sheet.pdf)
explains the configuration in the startup script and the database links.

## Feedback / Reporting issues

Please use the OPC UA Device Support Module's GitHub
[issue tracker](https://github.com/ralphlange/opcua/issues).

## License

This example application is part of the OPC UA Device Support module
that is distributed subject to a Software License Agreement found
in file LICENSE that is included with its distribution.
