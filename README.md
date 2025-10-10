<a target="_blank" href="http://semver.org">![Version][badge.version]</a>
<a target="_blank" href="https://app.codacy.com/gh/epics-modules/opcua/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade">![Codacy grade][badge.codacy]</a>

[![open62541][badge.open62541-linux]](https://github.com/epics-modules/opcua/actions/workflows/ci-build-open62541-linux.yml)
[![UA SDK][badge.uasdk-linux]](https://github.com/epics-modules/opcua/actions/workflows/ci-build-uasdk-linux.yml)

[![open62541][badge.open62541-win]](https://github.com/epics-modules/opcua/actions/workflows/ci-build-open62541-win.yml)
[![UA SDK][badge.uasdk-win]](https://github.com/epics-modules/opcua/actions/workflows/ci-build-uasdk-win.yml)

# opcua - EPICS Device Support for OPC UA

[EPICS](https://epics-controls.org)
Device Support module for interfacing to the OPC UA protocol.
The architecture allows using different implementations
of the low level client library.

Linux and Windows native builds are supported.

## Status

The module is under development, but releases are stable and robust.

There are two choices for the low-level OPC UA client library:

1. The free open source client of the [open62541 project SDK][open62541.sdk]. \
   This integration is relatively new (added 2024)
   and is gradually introduced in production at more and more places.

2. The commercially available
   [Unified Automation C++ Based OPC UA Client SDK][unified.sdk]. \
   This is the original implementation.

## Prerequisites

* A C++ compiler that supports the C++11 standard. \
  Microsoft Visual C++ needs to be from Visual Studio 2015 or newer.
  g++ needs to be 4.6 or above.

* [EPICS Base](https://epics-controls.org/resources-and-support/base/)
  3.15 (>= 3.15.7) or EPICS 7 (>= 7.0.4).

* The [gtest module](https://github.com/epics-modules/gtest) if you want
  to compile and run the unit tests that are based on Google Test.

### Using the Unified Automation Client SDK

* Unified Automation C++ Based [OPC UA Client SDK][unified.sdk]
  (1.5/1.6/1.7 are supported, as well as their evaluation bundles;
  1.8 is having trouble).

* For OPC UA security support (authentication/encryption), you need
  openssl/libcrypto on your system - both when compiling the SDK and when
  generating any binaries (IOCs).

* In `CONFIG_SITE.local`, set `UASDK` to the path of your SDK installation.

* For more details, refer to the `README.md` in the
  [`devOpcuaSup/UaSdk`][uasdk.dir] directory.

### Using the open62541 SDK

* The [open62541 SDK][open62541.sdk] \
  Choose a recent release from series 1.2 or 1.3
  (release seriens 1.4 is not supported yet).
  Download the sources using GitHub releases, not through the project website.

* For OPC UA security support (authentication/encryption), you need
  openssl/libcrypto on your system - both when compiling the SDK and when
  generating any binaries (IOCs).

* In `CONFIG_SITE.local`, set `OPEN62541` to the path of your SDK installation.

* For more details, refer to the `README.md` in the
  [`devOpcuaSup/open62541`][open62541.dir] directory.

## Building the module

This is a standard EPICS module.

Inside the `configure` subdirectory or one level above the TOP location
(TOP is where this README file resides), create a file `RELEASE.local`
that sets `EPICS_BASE` and `GTEST` to the absolute paths inside your EPICS
installation. The `GTEST` module is needed to compile and run the tests.
Not defining it produces a clean build, but without any tests.

Configure the compiler on Linux to use the C++11 standard by adding

```makefile
USR_CXXFLAGS_Linux += -std=c++11
```

to the `CONFIG_SITE` file (or one of the host/target specific site
configuration files). \
It is preferable to set this option globally in EPICS Base.

The configuration necessary when building against a specific client library
is documented in the `README.md` file inside the respective subdirectory of
`devOpcuaSup`.

## Using the module

IOC applications that use the module need to

* add an entry to the Device Support module in their `RELEASE.local` file
* include `opcua.dbd` when building the IOC's DBD file
* include `opcua` in the support libraries for the IOC binary.

## Documentation

Sparse, but getting better.

The documentation folder of the Device Support module contains the
[Requirements Specification (SRS)][requirements.pdf] giving an introduction
and the list of requirements that should convey a good idea of the planned
features.

The [Cheat Sheet][cheatsheet.pdf] explains the configuration in the startup
script and the database links.

## Binary Distribution

Please look at the "Assets" sections of specific releases
on the [release page](https://github.com/epics-modules/opcua/releases)
for the binary distribution tars.
These tars contain an EPICS module 
with a binary Linux shared library (`libopcua.so.<version>`)
that contains (embeds) the Unified Automation low-level client.

In your build setup, the module from the binary distribution
can be used like a support module built from source.
The binary device support is fully functional
and can be used without limitations or any fees.

You need to download the binary distribution tar that matches
your Linux distribution and the exact EPICS Base version,
else you will not be able to create IOCs.

## Feedback / Reporting issues

Please use the GitHub project's
[issue tracker](https://github.com/epics-modules/opcua/issues).

## Credits

This module is based on extensive
[prototype work](https://github.com/bkuner/opcUaUnifiedAutomation)
by Bernhard Kuner (HZB/BESSY) and uses ideas and code snippets from
Michael Davidsaver (Osprey DCS).

Support for the open62541 client library
and many fixes and new features were added by Dirk Zimoch (PSI)
with additional help from Carsten Winkler (HZB/BESSY).

The end-to-end test suite is a reduced clone of the test application
that has been developed at the ESS by Ross Elliot and Karl Vestin.

## License

This module is distributed subject to a Software License Agreement found
in the file `LICENSE` that is included with this distribution.

<!-- Links -->
[badge.version]: https://img.shields.io/github/v/release/epics-modules/opcua?sort=semver
[badge.codacy]: https://app.codacy.com/project/badge/Grade/ec0d53f8285249d394b3af067acf2ad4

[badge.uasdk-win]: https://github.com/epics-modules/opcua/actions/workflows/ci-build-uasdk-win.yml/badge.svg
[badge.uasdk-linux]: https://github.com/epics-modules/opcua/actions/workflows/ci-build-uasdk-linux.yml/badge.svg
[badge.open62541-win]: https://github.com/epics-modules/opcua/actions/workflows/ci-build-open62541-win.yml/badge.svg
[badge.open62541-linux]: https://github.com/epics-modules/opcua/actions/workflows/ci-build-open62541-linux.yml/badge.svg

[unified.sdk]: https://www.unified-automation.com/products/client-sdk/c-ua-client-sdk.html
[open62541.sdk]: https://open62541.org/

[uasdk.dir]: https://github.com/epics-modules/opcua/tree/master/devOpcuaSup/UaSdk
[open62541.dir]: https://github.com/epics-modules/opcua/tree/master/devOpcuaSup/open62541
[requirements.pdf]: https://docs.google.com/viewer?url=https://raw.githubusercontent.com/epics-modules/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20SRS.pdf
[cheatsheet.pdf]: https://docs.google.com/viewer?url=https://raw.githubusercontent.com/epics-modules/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20Cheat%20Sheet.pdf
