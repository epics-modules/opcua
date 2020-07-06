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

|   Name   | Data Type |      OPC UA Node Name       |
| -------- | --------- | --------------------------- |
| myInt    | Int       | `"Data_block_1"."myInt"`    |
| myWord   | Word      | `"Data_block_1"."myWord"`   |
| myString | String    | `"Data_block_1"."myString"` |
| myBool   | Bool      | `"Data_block_1"."myBool"`   |
| myDouble | Double    | `"Data_block_1"."myDouble"` |

The double quotes are part of the OPC UA node name and have to be escaped
inside EPICS database field values.

## Use a GUI to check

When developing, using a professional GUI client for OPC UA is strongly
suggested.
The free [UaExpert][uaexpert] tool from Unified Automation is a good choice.

## S7-1500 Firmware versions

With every update of the firmware, Siemens changes the server behavior a lot,
increasing limitations, fixing bugs and improving the "user experience".

It is generally advisable to update the PLC to the newest available firmware.

Regularly check the [detailed release notes][release_notes_1500] to see what
things were fixed.

Known issues:

*   Firmware V2.6.0 is needed for subscriptions on registered items to work.

*   All known firmware versions have a limit of 1000 items per low-level
    OPC UA service call. To enforce this limit in the client IOC, call
    ```sh
    opcuaSetOption <session_name> batch-nodes 1000
    ```
    for every S7-1500 session that you want to use for more than 1000 items.

## OPC UA Device Support Documentation

The documentation folder of the Device Support module contains the
[Requirements Specification (SRS)][requirements.pdf] giving an introduction
and the list of requirements that should convey a good idea of the planned
features.

The [Cheat Sheet][cheatsheet.pdf] explains the configuration in the startup
script and the database links.

## Feedback / Reporting issues

Please use the OPC UA Device Support Module's GitHub
[issue tracker](https://github.com/ralphlange/opcua/issues).

## License

This example application is part of the OPC UA Device Support module
that is distributed subject to a Software License Agreement found
in file LICENSE that is included with its distribution.

<!-- Links -->
[requirements.pdf]: https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20SRS.pdf
[cheatsheet.pdf]: https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20Cheat%20Sheet.pdf
[uaexpert]: https://www.unified-automation.com/products/development-tools/uaexpert.html
[release_notes_1500]: https://support.industry.siemens.com/cs/document/109478459/firmware-update-s7-1500-cpus-incl-displays-and-et200-cpus-(et200sp-et200pro)?dti=0&lc=en-WW
