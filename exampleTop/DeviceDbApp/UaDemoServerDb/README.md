# Unified Automation Example Server (UaServerCpp)

All bundles by Unified Automation provide an example server for
demonstration and testing.

## Start the Example Server

Change into the `bin` subdirectory of the UA software bundle and
start the `uaservercpp` binary.

## Provided Variables

When developing, using a professional GUI client for OPC UA is strongly
suggested.
The free [UaExpert][uaexpert] tool from Unified Automation is a good choice.

Use that GUI client to browse the server:
Under `Demo`, there are examples of all OPC UA types and features that the
Unified Automation stack supports. The example IOC connects to a number
of selected variables.

## OPC UA Security

The example server supports all modes of OPC UA security and is a good
way to check your certificate setup.

The server's PKI file store is located under the `bin` subdirectory of
the UA software bundle inside `pkiserver`.

The PKI file store for verification of user authentication certificates
is located under the same `bin` subdirectory inside `pkiuser`.

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
