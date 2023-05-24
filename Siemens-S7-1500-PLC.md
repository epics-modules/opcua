Siemens S7 1500 series PLCs provide an embedded OPC UA server running on the PLC itself.
The performance and capabilities vary with the size of the PLC.

You need to acquire a license for every PLC that you want to enable the OPC UA server on.
(The cost of the license also varies with the three different sizes of the 1500s.)

# Prepare Your PLC in TIA Portal

## Enable the OPC UA server

*   Inside the "Properties" / "General" tab, under "OPC UA" / "Server", enable the OPC UA server.
*   Take note of the URL for your server.
*   Under "Runtime licenses" / "OPC UA", select the type of license you acquired for your PLC.

_The PLC has to be disconnected for this configuration._

## Mark the items you want to access

In the list view of a Data Block, the checkboxes for "Accessible from HMI/OPC UA"
and "Writable from HMI/OPC UA" determine which data are accessible and which are writable.

# S7-1500 Firmware Versions

With every update of the firmware, Siemens changes the server behavior a lot,
increasing limitations, fixing bugs and improving the "user experience".

It is generally advisable to update the PLC to the newest available firmware.

Regularly check the [detailed release notes][release_notes_1500] on the Siemens website
to see what things were fixed.

Known issues:

*   Firmware V2.6.0 or higher is needed for subscriptions on registered items to work.

*   All known firmware versions have a limit of 1000 items per low-level
    OPC UA service call. To enforce this limit in the client IOC, call
    ```sh
    opcuaOption <pattern> batch-nodes=1000
    ```
    with a _pattern_ matching all S7-1500 sessions that you want to use for more than 1000 items.
    Doing so will split up larger requests into batches of that size.

# Links

*   [System Limits of OPC UA Server for S7-1500 CPUs][system_limits_1500] 

<!-- Links -->
[release_notes_1500]: https://support.industry.siemens.com/cs/document/109478459/firmware-update-s7-1500-cpus-incl-displays-and-et200-cpus-(et200sp-et200pro)?dti=0&lc=en-WW
[system_limits_1500]: https://support.industry.siemens.com/cs/document/109755846/what-are-the-system-limits-of-the-opc-ua-server-with-s7-1500-and-s7-1200-?dti=0&lc=en-BH