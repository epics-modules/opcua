# Open62541 Client C Library

## Status

:warning:
This module is under development and still experimental.
Please contact the authors [Dirk Zimoch](mailto:dirk.zimoch@psi.ch) and [Ralph Lange](mailto:ralph.lange@gmx.de) for details.
:warning:

## Prerequisites

*   [Open62541](https://www.open62541.org/) C library
    (1.3 has been tested; system packages also work).
*   Cmake (3.x) if you're building Open62541 from sources.
*   For OPC UA security support (authentication/encryption), we suggest using the openssl plugin for Open62541. For that to work, you need openssl on your system - both when compiling the client library and when generating any binaries (IOCs).
    The name of the package you have to install depends on the Linux distribution: `openssl-devel` on RedHat/CentOS/Fedora, `libssl-dev` on Debian/Ubuntu.
    Use the `CONFIG_SITE.local` file to enable the OPC UA Security option.

## Building Open62541

*   Preface: The Open62541 project is focused on the server implementation of OPC UA. The client functionality is certainly present, complete and usable, but it does not get the attention that the server parts get.
    
*   The cmake build of Open62541 creates a static library by default. This type of library is needed for the EMBED type of Device Support build (see below),
    
    ```Shell
    cmake .. -DBUILD_SHARED_LIBS=ON ...
    ```
    
    To create shared libraries, build the Open62541 library setting the option `BUILD_SHARED_LIBS=OFF`.
    
    You can create both types by running two builds with different configurations in the same build directory. Remove the CMakeCache in between to get clean configurations.
    
*   Open62541 supports multiple low-level libraries to implement OPC UA Security. The most widely used option (that also keeps things simple by staying in line with the UA SDK client) is based on openssl. We suggest to select that by setting the option `UA_ENABLE_ENCRYPTION=OPENSSL`.

*   Given all that, a reasonable option setting would be:

    ```shell
    cmake .. -DBUILD_SHARED_LIBS=ON \
             -DCMAKE_BUILD_TYPE=RelWithDebInfo\
             -DUA_ENABLE_ENCRYPTION=OPENSSL
    ```

    There are a lot of other options available, but most of them affect only the server part and are not relevant for use of the client.

*   The default installation location is below `/usr/local`. Normally, this is a system location, so that the deploy mode setting (see below) would be `SYSTEM`.
    If you want to install into another location, set the option `CMAKE_INSTALL_PREFIX=/other/location`. In that case, the deploy mode setting (see below) would be `PROVIDED` and the library location needs to be set. (The same is true if you want to use the Open62541 library in its build location.)

*   After running cmake, run `make` followed by `make install`. Installation into a system location will need root permission.

## Building the device support module

Inside the `configure` subdirectory or one level above the TOP location,
create a file `RELEASE.local` that sets `EPICS_BASE` to the absolute path
of your EPICS installation.

Inside the `configure` subdirectory or one level above the TOP location,
create a file `CONFIG_SITE.local` that sets the absolute path of your Open62541
installation as well as its build and deploy features if necessary.

```Makefile
# Path to the Open62541 C++ installation
OPEN62541 = /other/location

# How the Open62541 shared libraries are deployed
#   SYSTEM = shared libs are in a system location
#   PROVIDED = shared libs are in $(OPEN62541_SHRLIB_DIR)
#   INSTALL = shared libs are installed (copied) into this module
#   EMBED = link Open62541 code statically into libopcua,
#           the Open62541 libraries are not required on target system
OPEN62541_DEPLOY_MODE = PROVIDED
OPEN62541_LIB_DIR = $(OPEN62541)/lib
OPEN62541_SHRLIB_DIR = $(OPEN62541_LIB_DIR)
# How the Open62541 libraries were built
OPEN62541_USE_CRYPTO = YES
```

Note: On Windows, paths must include "short names" where needed, e.g.
```Makefile
OPEN62541 = C:/PROGRA~2/Open62541/
```

The Open62541 related configuration only has to be done in this module, which creates a `CONFIG_OPCUA` file that is automatically included by all downstream modules (those creating IOCs), so that the configuration is always consistent.

## Feedback / Reporting issues

Please use the GitHub project's [issue tracker](https://github.com/ralphlange/opcua/issues).

## Known issues

n/a

## Credits

The adaption of the Open62541 client library has been done by Dirk Zimoch (PSI) with help from Carsten Winkler (HZB/BESSY II). Additional EPICS build system magic by Ralph Lange (ITER).

## License

This module is distributed subject to a Software License Agreement found
in file LICENSE that is included with this distribution.

<!-- Links -->