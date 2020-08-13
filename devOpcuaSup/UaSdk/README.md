# Unified Automation C++ Based OPC UA Client SDK

## Status

:warning:
This module is under development.
Please contact the author [Ralph Lange](mailto:ralph.lange@gmx.de) for details.
:warning:

## Prerequisites

*   Unified Automation C++ Based [OPC UA Client SDK][unified.sdk]
    (1.5/1.6/1.7 are supported, as well as their evaluation bundle).

*   For OPC UA security support (authentication/encryption), you need
    libcrypto on your system - both when compiling the SDK and when generating
    any binaries (IOCs).
    The name of the package you have to install depends on the Linux distribution:
    `openssl-devel` on RedHat/CentOS/Fedora, `libssl-dev` on Debian/Ubuntu.
    Use the `CONFIG_SITE.local` file in the module where the binary is created
    to set this option.

*   The OPC UA Client SDK sets `BUILD_SHARED_LIBS=OFF` as default.
    To create shared SDK libraries, build the SDK using
    ```Shell
    ./buildSdk.sh -s ON
    ```
    In version 1.5.5 of the SDK, the `buildSdk.sh` build script does not apply
    the `-s ON` setting the the stack component. To fix this and create a complete
    shared library set of the SDK, apply the following patch:
    ```Diff
    --- buildSdk.sh
    +++ buildSdk.sh
    @@ -95,7 +95,7 @@
         cd $UASDKDIR/build$config || { echo "cd $UASDKDIR/build$config failed."; exit 1; }
         # create the Makefile using CMake
         # Just create only the SDK Makefiles
    -    cmake "$TOOLCHAIN" "$OPTION" -DBUILD_EXAMPLES=off -DBUILD_UACLIENTCPP_APP=off -DBUILD_UASERVERCPP_APP=off -DENABLE_GCC_FORTIFY_SOURCE=off -DCMAKE_BUILD_TYPE=$config -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS -DCMAKE_INSTALL_PREFIX=$CMAKE_INSTALL_PREFIX $UASDKDIR
    +    cmake "$TOOLCHAIN" "$OPTION" -DBUILD_EXAMPLES=off -DBUILD_UACLIENTCPP_APP=off -DBUILD_UASERVERCPP_APP=off -DENABLE_GCC_FORTIFY_SOURCE=off -DCMAKE_BUILD_TYPE=$config -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS -DBUILD_SHARED_STACK=$BUILD_SHARED_LIBS -DCMAKE_INSTALL_PREFIX=$CMAKE_INSTALL_PREFIX $UASDKDIR
         # build
         make "$JOBS" || { echo "make failed."; exit 1; }
         # install
    ```

*   Using the evaluation binary bundle of Unified Automation (free download)
    is supported.
    However, the EVAL bundle contains shared-only libraries (namely the stack
    component, see above), so only shared builds of this Device Support will
    work.

## Building the device support module

Inside the `configure` subdirectory or one level above the TOP location,
create a file `RELEASE.local` that sets `EPICS_BASE` to the absolute path
of your EPICS installation.

Inside the `configure` subdirectory or one level above the TOP location,
create a file `CONFIG_SITE.local` that sets the absolute path of your SDK
installation as well as the SDK build and deploy features if necessary.
```Makefile
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
```Makefile
UASDK = C:/PROGRA~2/UnifiedAutomation/UaSdkCppBundleEval
```

The SDK related configuration only has to be done in this module,
which creates a `CONFIG_OPCUA` file that is automatically included by all
downstream modules, so that the configuration is always consistent.

When using the evaluation bundle of the SDK, only regular (shared) builds
are supported.
Do not set `STATIC_BUILD = YES` in your configuration.

## Feedback / Reporting issues

Please use the GitHub project's
[issue tracker](https://github.com/ralphlange/opcua/issues).

## Known issues

Reported by Shi Li (ASIPP): \
The C++ SDK version 1.7.1 has a bug that shows as monitored items not
getting updates anymore after the server has disconnected and reconnected.
A reboot of the IOC restores good behaviour until the server disconnects
again. Updating the C++ SDK to version 1.7.2 resolves the issue.

## Credits

This module is based on extensive
[prototype work](https://github.com/bkuner/opcUaUnifiedAutomation)
by Bernhard Kuner (HZB/BESSY II) and uses ideas and code snippets from
Michael Davidsaver (Osprey DCS).

## License

This module is distributed subject to a Software License Agreement found
in file LICENSE that is included with this distribution.

<!-- Links -->
[unified.sdk]: https://www.unified-automation.com/products/client-sdk/c-ua-client-sdk.html
