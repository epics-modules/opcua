# opcua - EPICS Device Support for OPC UA

[EPICS](https://epics-controls.org) Device Support module for interfacing
to the OPC UA protocol. The architecture allows supporting different
implementations of the low level client library.

## Status

:warning:
This module is under development.
Please contact the author <ralph.lange@gmx.de> for details.
:warning:

The first (and - at this time - only) supported OPC UA client library is the
commercially available Unified Automation C++ Based OPC UA Client SDK.

## Prerequisites

* A C++ compiler that supports the C++11 standard.

* [EPICS Base](https://epics-controls.org/resources-and-support/base/)
  3.15.5 (and up; EPICS 7 is supported).

### Using the Unified Automation Client SDK

* Unified Automation C++ Based
  [OPC UA Client SDK](https://www.unified-automation.com/products/client-sdk/c-ua-client-sdk.html)
  1.5 (and up; 1.6 is supported).

* For OPC UA security support (authentication/encryption), you need
  libcrypto on your system - both when compiling the SDK and when generating
  any binaries (IOCs).
  The name of the package you have to install depends on the Linux distribution:
  openssl-devel on RedHat/CentOS/Fedora, libssl-dev on Debian/Ubuntu.
  Use the `CONFIG_SITE.local` file in the module where the binary is created
  to set this option.

* Since UASDK v1.6.3 has BUILD_SHARED_LIBS=OFF as default,
  one should use ON via ```./buildSdk.sh -s ON```.

## Building the module

This is a standard EPICS module.

Inside the `configure` subdirectory or one level above the TOP location
(TOP is where this README file resides), create a file `RELEASE.local`
that sets `EPICS_BASE` to the absolute path of your EPICS installation.

### Using the Unified Automation Client SDK

Inside the `configure` subdirectory or one level above the TOP location,
create a file `CONFIG_SITE.local` that sets the absolute path of your SDK
installation as well as the SDK build and deploy features if necessary.
```
# Path to the Unified Automation OPC UA C++ SDK
UASDK = /usr/local/opcua/uasdkcppclient-v1.5.3-346/sdk

# How the Unified Automation SDK shared libraries are deployed
#   SYSTEM = shared libs are in a system location
#   PROVIDED = shared libs are in $(UASDK_DIR)
#   INSTALL = shared libs are installed (copied) into this module
UASDK_DEPLOY_MODE = PROVIDED
UASDK_DIR = $(UASDK)/lib
# How the Unified Automation SDK libraries were built
UASDK_USE_CRYPTO = YES
UASDK_USE_XMLPARSER = YES
```

Note: On Windows, paths must include "short names" where needed, e.g.
```
UASDK = C:/PROGRA~2/UnifiedAutomation/UaSdkCppBundleEval
```

## Using the module

IOC applications that use the module need to

* add an entry to the Device Support module in their `RELEASE.local` file
* include `opcua.dbd` when building the IOC's DBD file
* include `opcua` in the support libraries for the IOC binary.

## Documentation

Sparse.

The documentation folder contains the
[Requirements Specification (SRS)](https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20SRS.pdf)
giving an introduction and the list of requirements that should convey a good
idea of the planned features.

The [Cheat Sheet](https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20Cheat%20Sheet.pdf)
explains the configuration in the startup script and the database links.

## Feedback / Reporting issues

Please use the GitHub project's
[issue tracker](https://github.com/ralphlange/opcua/issues).

## Credits

This module is based on extensive
[prototype work](https://github.com/bkuner/opcUaUnifiedAutomation)
by Bernhard Kuner (HZB/BESSY II) and uses ideas and code snippets from
Michael Davidsaver (Osprey DCS).

## License

This module is distributed subject to a Software License Agreement found
in file LICENSE that is included with this distribution.
