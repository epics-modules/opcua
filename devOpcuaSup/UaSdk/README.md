# Unified Automation C++ Based OPC UA Client SDK

## Status

:warning:
This module is under development.
Please contact the author [Ralph Lange](mailto:ralph.lange@gmx.de) for details.
:warning:

## Prerequisites

*   Unified Automation C++ Based [OPC UA Client SDK][unified.sdk]
    (1.5/1.6/1.7 are supported, as well as their evaluation bundles).
    
*   Windows and Linux builds are supported.

*   Using the evaluation binary bundles by Unified Automation (free download)
    is supported.
    However, the EVAL bundle contains a shared library (DLL), namely the stack
    component, see below, so only shared builds of this Device Support will
    work.

### Linux

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
    In many versions of the SDK, the `buildSdk.sh` build script does not apply
    the `-s ON` setting the the stack component. To fix this and create a complete
    shared library set of the SDK, apply the following patch (shown for 1.5.5):
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

### Windows

*   Binary libraries (DLLs) of openssl and libxml2 are provided under the
    `third-party` directory of the SDK bundle.

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
#   PROVIDED = shared libs are in $(UASDK_SHRLIB_DIR)
#   INSTALL = shared libs are installed (copied) into this module
#   EMBED = link SDK code statically into libopcua,
#           SDK libraries are not required on target system
UASDK_DEPLOY_MODE = PROVIDED
UASDK_LIB_DIR = $(UASDK)/lib
UASDK_SHRLIB_DIR = $(UASDK_LIB_DIR)
# How the Unified Automation SDK libraries were built
UASDK_USE_CRYPTO = YES
UASDK_USE_XMLPARSER = YES
```

The SDK related configuration only has to be done in this module,
which creates a `CONFIG_OPCUA` file that is automatically included by all
downstream modules, so that the configuration is always consistent.

When using the evaluation bundle of the SDK, only regular (shared) builds
are supported.
Do not set `STATIC_BUILD = YES` in your configuration.

### Windows Specifics

Windows builds should use `UASDK_DEPLOY_MODE = PROVIDED` and set `UASDK`
to the location of the Unified Automation bundle.

Paths must include Windows "short names" where needed, e.g.
```Makefile
UASDK = C:/PROGRA~2/UnifiedAutomation/UaSdkCppBundleEval
```

If you do not have Administrator rights or if you want to unpack multiple
versions of the UA SDK bundle in parallel, [UniExtract2][uniextract2] is your
friend.

For any created binaries to successfully find the EPICS and SDK DLLs,
both the binary locations of the EPICS Base installation and the UA SDK
bundle installation have to be added to the PATH.
This needs to be done before starting the build, as some created binaries
(tests) are being run as part of the build.

Please use your specific locations and "short names" when running a command
like
```Shell
path %PATH%C:\Users\foobar\EPICS\base\7.0.4.1;C:\PROGRA~2\UnifiedAutomation\UaSdkCppBundleEval\bin;
```

Alternatively, you could set `UASDK_DEPLOY_MODE = INSTALL`, which copies all
needed DLLs into the binary location of your IOC binary.
Be aware that over time, this approach will lead to a large number of possibly
different DLLs with the same name being used concurrently on the system -
a situation that is hard to maintain.
If you want your IOC binaries to be deployable without depending on
specific DLLs being present on the target system, you should consider linking
your IOCs statically. (As stated above, static builds are not available when
using the evaluation bundles.)

## Feedback / Reporting issues

Please use the GitHub project's
[issue tracker](https://github.com/ralphlange/opcua/issues).

## Known issues

Reported by Shi Li (ASIPP): \
The C++ SDK version 1.7.1 has a bug that shows as monitored items not
getting updates anymore after the server has disconnected and reconnected.
A reboot of the IOC restores good behaviour until the server disconnects
again. Updating the C++ SDK to version 1.7.2 resolves the issue.

Reported by Carsten Winkler (HZB/BESSY): \
On Windows 10, you may experience a version mismatch between the openssl DLLs
that are part of the Windows system and a different version of the same DLLs
that are part of the UA SDK bundle.
In that case, copying the DLLs from the UA SDK into the same directory as the
unit tests (`...\unitTestApp\src\O.win...`) and your executable (IOC binary)
seems to be the only reasonable workaround.
(Consider setting `UASDK_DEPLOY_MODE = INSTALL` in that case.)

## Credits

This module is based on extensive
[prototype work](https://github.com/bkuner/opcUaUnifiedAutomation)
by Bernhard Kuner (HZB/BESSY) and uses ideas and code snippets from
Michael Davidsaver (Osprey DCS).
Support for Windows builds with help from Carsten Winkler (HZB/BESSY).

## License

This module is distributed subject to a Software License Agreement found
in file LICENSE that is included with this distribution.

<!-- Links -->
[unified.sdk]: https://www.unified-automation.com/products/client-sdk/c-ua-client-sdk.html
[uniextract2]: https://github.com/Bioruebe/UniExtract2
