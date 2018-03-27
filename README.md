# opcua - EPICS Device Support for OPC UA

EPICS Device Support module for interfacing to the OPC UA protocol
using the Unified Automation C++ Based
[OPC UA Client SDK](https://www.unified-automation.com/products/client-sdk/c-ua-client-sdk.html).

## Status

:warning:
This module is under development.
Please contact the author <ralph.lange@gmx.de> for details.

## Prerequisites

* EPICS Base 3.15.5 (and up).

* Unified Automation C++ Based OPC UA Client SDK.
  This device support has been developed using the 1.5.x series.

* For OPC UA security support (authentication/encryption), you need
  libcrypto on your system - both when compiling the SDK and when generating
  any binaries (IOCs).
  The name of the package you have to install depends on the Linux distribution:
  openssl-devel on RedHat/CentOS/Fedora, libssl-dev on Debian/Ubuntu.
  Use the `CONFIG_SITE.local` file in the module where the binary is created
  to set this option.

## License
This module is distributed subject to a Software License Agreement found
in file LICENSE that is included with this distribution.
