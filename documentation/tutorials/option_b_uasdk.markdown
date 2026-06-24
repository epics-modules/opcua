(build-uasdk)=
# *Option B:* Unified Automation C++ Client SDK

The Unified Automation SDK (UASDK) was the first and original low-level client
used in the OPC UA Device Support.
It is a very stable and complete "gold-standard" implementation.

Unified Automation offers a freely available evaluation bundle (binary),
which offers full functionality
with a running time limited to one hour.

## Status

Support for this low-level client is stable,
with some expected development still to be happening.

Please contact the authors
[Dirk Zimoch](mailto:dirk.zimoch@psi.ch) and
[Ralph Lange](mailto:ralph.lange@gmx.de) for details.

## Prerequisites

* Unified Automation C++ Based [OPC UA Client SDK][uasdk]
  (1.5/1.6/1.7 are supported,
  as well as their [evaluation bundles][uasdk-eval]).

* Windows and Linux builds are supported.

* The evaluation binary bundles by Unified Automation
  (free download) are supported.
  However, the EVAL bundle contains a shared library (DLL),
  namely the stack component, see below,
  so only shared builds of the Device Support will work.

## On Linux

* For OPC UA security support (authentication/encryption),
  you need `libcrypto` on your system -
  both when compiling the SDK and when generating any binaries (IOCs).
  The name of the package you have to install
  depends on the Linux distribution:
  `openssl-devel` on RedHat/CentOS/Fedora,
  `libssl-dev` on Debian/Ubuntu.

* The UA Client SDK sets `BUILD_SHARED_LIBS=OFF` as default.
  To create shared SDK libraries, build the SDK using

  ```Shell
  ./buildSdk.sh -s ON
  ```

  In many versions of the SDK,
  the `buildSdk.sh` build script does not apply the `-s ON` setting
  to the stack component.
  To fix this and create a complete shared library set of the SDK,
  apply the following patch (shown for 1.5.5):

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

  You can build the shared and the static versions of the SDK
  in the same location (there are no artifact conflicts).
  This allows full flexibility for the Device Support module
  and your IOC applications, i.e., static and shared builds.

## On Windows

* Extract the SDK to a known location,
  e.g., `/opt/unifiedautomation/UaSdkCppBundle`
  or `C:\Program Files\UnifiedAutomation\UaSdkCppBundle`.
  This is where the Makefile variable `UASDK`
  should point to (see below).
* Binary libraries (DLLs and headers) of matching versions
  for openssl and libxml2 are provided
  under the `third-party` directory of the SDK bundle.

(devsup-conf-uasdk)=
## Configure the Device Support Module

Inside the `configure` subdirectory or one level above the TOP location,
create a file `RELEASE.local`
that sets `EPICS_BASE` to the absolute path of your EPICS installation.

Inside the `configure` subdirectory or one level above the TOP location,
create a file `CONFIG_SITE.local`
that sets the absolute path of your SDK installation
as well as the SDK build and deploy features if necessary.

```makefile
# Path to the Unified Automation OPC UA C++ SDK
UASDK = /usr/local/opcua/uasdkcppclient-v1.5.3-346/sdk

# How the Unified Automation SDK shared libraries are deployed
# (See the Reference for more details and recommendations)
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

See [Deployment Modes](deployment-modes) for details and recommendations on which mode to choose for your platform and situation.

This SDK related configuration is only needed in the Device Support module.
During the build,
it creates a `CONFIG_OPCUA` file that is automatically included
by all downstream (IOC-generating) modules and applications.
This ensures that the configuration is always consistent.

:::{caution}
When using the evaluation bundle of the SDK,
only regular (shared) builds are supported.
Do not set `STATIC_BUILD = YES` in your configuration.
:::

### Windows Specifics

Windows builds should use `UASDK_DEPLOY_MODE = PROVIDED`
and set `UASDK` to the location of the Unified Automation bundle.

All paths in EPICS build configuration files
must use Windows "short names" where needed, e.g.

```makefile
UASDK = C:/PROGRA~2/UnifiedAutomation/UaSdkCppBundleEval
```

In your `CONFIG_SITE.local`,
you also need to define the locations
of the supplied third-party dependencies.

```makefile
# Prerequisites: libxml2 openssl
LIBXML2 = $(UASDK)/third-party/win64/vs2015/libxml2
OPENSSL = $(UASDK)/third-party/win64/vs2015/openssl
```

:::{caution}
Different versions of the Unified Automation bundle
have these libraries in different folders, under different names.
Check `configure/CONFIG_SITE.Common.win32-x86`
and adapt the settings to your situation.
:::

If you do not have Administrator rights
or if you want to unpack multiple versions of the UA SDK bundle,
[UniExtract2][uniextract2] is your friend.

:::{caution}
The evaluation bundle only contains binary libraries
to be used with the MSVC compilers.
MinGW can not be used with the libraries from the bundle,
as the C++ name mangling is different.
:::

As always on Windows,
the build environment as well as the runtime environment (for any IOC)
need to have the locations of all DLLs in the PATH
to be able to run binaries (also applies to the unit tests).
You will need to add the appropriate locations for EPICS Base,
the opcua module, the UASDK library and all other dependencies you installed.
Alternatively,
you can put copies of all needed DLLs into the location of the binaries.
(Remember that `make distclean` will remove those copies.)

:::{caution}
Over time, Windows systems will have a large number
of possibly different DLLs with the same names and/or versions
being used concurrently on the system -
a situation that is hard to maintain
and known as [DLL Hell](https://en.wikipedia.org/wiki/DLL_Hell).

If you want your IOC binaries to be deployable
without depending on specific DLLs being present on the target system,
consider linking your IOCs statically.
(Note that static builds are not available when using the evaluation bundles.)
:::

On newer Windows systems,
the Windows system version of the openssl DLLs
are not compatible with the UA SDK bundled versions (see below).
Copy the vendor-supplied DLLs
next to the binaries to avoid a mismatch.

## Known issues

Reported by Shi Li (ASIPP):
The C++ SDK version 1.7.1 has a bug
that shows as monitored items not getting updates anymore
after the server has disconnected and reconnected.
A reboot of the IOC restores good behavior
until the server disconnects again.
Updating the C++ SDK to version 1.7.2 resolves the issue.

## Credits

This option is based
on extensive [prototype work](https://github.com/bkuner/opcUaUnifiedAutomation)
by Bernhard Kuner (HZB/BESSY).
Support for Windows builds with help from Carsten Winkler (HZB/BESSY).

<!-- Links -->

[uasdk-eval]: https://www.unified-automation.com/downloads/opc-ua-sdks.html
[uniextract2]: https://github.com/Bioruebe/UniExtract2
