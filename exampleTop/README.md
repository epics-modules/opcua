# exampleTop - Example Application for OPC UA Support

This is an example application that shows how to build an IOC
that connects to an OPC UA server.

The only intention is to get you started.
The examples are not necessarily intended as a template for a real world
application. They don't cover all features, and they are not the one
and only way to do things.

## Preparing the Module

There are two ways to build the example application:

1.  __As an embedded module inside the OPC UA Support.__
    This is the original setup. This module (named `exampleTop`) is built
    inside the Device Support module. All configuration is set up to use the
    configuration of the parent module.
    The example is being built as part of the hierarchical parent module build.
    Obviously, you can also just `cd` into the example and rebuild it
    without touching the parent module.

    Note: If you set `INSTALL_LOCATION` in the parent (Device Support) module,
    you have to create a file `RELEASE.local` inside `configure` (or at one
    of the other usual places) that sets `OPCUA` to the absolute path
    (`INSTALL_LOCATION`) of the OPC UA Device Support

2.  __As a standalone module.__
    After copying the `exampleTop` module into your workspace (feel free to
    rename it), you will have to adapt its configuration.

    *   At the bottom of `configure/CONFIG_SITE` and `configure/RELEASE`,
        adjust the commented-out sections to match your case.

    *   Create a file `RELEASE.local` inside `configure` (or at one of the
        other usual places) that sets `OPCUA` and `EPICS_BASE` to the absolute
        path of the OPC UA Device Support and EPICS Base modules.

## Building the Module

Run `make` at the top level of the module.

## Preparing the IOC

Edit the example IOC startup script (`iocBoot/iocopcuaEx/st.cmd`):

*   Change the call to `opcuaCreateSession` to match your OPC UA server URL.

*   Uncomment the `dbLoadRecords` commands loading the example databases
    for your server, adapting their macro settings if necessary.

## Running the IOC

Run the IOC, loading the startup script.

Look at the database files to see which records are available.

## Contents of the Example Module

### TemplateDbSup

Template databases used by the device example databases.

`AnyServerDb` contains templates for namespace 0 (server namespace) that
should be implemented by any OPC UA server.

### DeviceDbApp

Example databases for different OPC UA servers.

Please refer to the README files inside the device directories for details.

The directory `.Db` is not being built, but intended as a template
when creating example databases for a new OPC UA device.

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

## Contributing

Contributions are welcome, preferably in the form of GitHub pull requests.

An example for a new OPC UA device should contain:

*   a `DeviceDbApp` sub-directory that creates all necessary EPICS databases

*   a `README.md` file describing how to prepare the device to work
    with the example application

*   a snippet for the startup script that loads the example databases.

## License

This example application is part of the OPC UA Device Support module
that is distributed subject to a Software License Agreement found
in file LICENSE that is included with its distribution.

<!-- Links -->
[requirements.pdf]: https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20SRS.pdf
[cheatsheet.pdf]: https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20Cheat%20Sheet.pdf
