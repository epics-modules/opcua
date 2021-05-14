# Unified Automation Example Server (UaServerCpp)

All bundles by Unified Automation provide an example server for
demonstration and testing.

## Start the Example Server

Change into the `bin` subdirectory of the UA software bundle and
start the `uaservercpp` binary.

## Provided Variables

Use a GUI client (`UaExpert` or similar) to browse the server.

Under `Demo`, there are examples of all OPC UA dfeatures that the
Unified Automation stack supports. The example IOC connects to a number
of selected variables.

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
