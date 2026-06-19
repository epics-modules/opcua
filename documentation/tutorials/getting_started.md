# Getting Started

This tutorial guides you through the process
of building the OPC UA Device Support
and setting up a basic EPICS IOC connection.

## Prerequisites

* **A C++ Compiler that supports the C++11 standard.**
  Microsoft Visual C++ needs to be from Visual Studio 2015 or newer.
  g++ needs to be 4.6 or above.
* **[EPICS Base][epics-base].**
  EPICS Base 3.15 (>= 3.15.7)
  or EPICS 7 (>= 7.0.4).
* **A low-level OPC UA client library.**
  Two options are supported:
  Either the free open-source client of the
  [open62541 project SDK][open62541]
  or the commercially available
  [Unified Automation C++ Based OPC UA Client SDK][uasdk].
* **_(Optional:)_ The [EPICS gtest module][epics-gtest].**
  Needed if you want to build and run the unit tests.

To use the OPC UA Device Support,
you first need to get and/or build the prerequisites,
then the Device Support module itself,
then your IOC application that uses it.

Linux and Windows native builds are supported.

## _Alternative:_ Binary Distribution for Linux

Please look at the "Assets" sections of specific releases
on the [release page](https://github.com/epics-modules/opcua/releases)
for the binary distribution tars.

These tars contain an EPICS module 
with a binary Linux shared library (`libopcua.so.<version>`)
that contains (embeds) the Unified Automation low-level client.
Its license allows distributing binaries royalty-free.

In your setup, the module from the binary distribution
can be used like a support module you built from source.
The binary device support is fully functional
and can be used without limitations or any fees.

You need to download the binary distribution tar that matches
your Linux distribution and the exact EPICS Base version,
else you will not be able to create IOCs.
(Failures show as linking errors in your application.)
