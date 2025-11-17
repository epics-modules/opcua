# Open62541 Client C Library

## Status

:warning:
This module is relatively new (2014),
but gaining popularity
and getting more and more hours of production use.

:warning:
Please contact the authors
[Dirk Zimoch](mailto:dirk.zimoch@psi.ch) and
[Ralph Lange](mailto:ralph.lange@gmx.de) for details.

## Prerequisites

*   [Open62541](https://www.open62541.org/) SDK / C library
    (v1.2 and v1.3 series have been tested; their system packages should also work.
    open62541 v1.4 changed the header file structure and does not work yet.)
    
*   Cmake (3.x) if you're building Open62541 from sources.

*   For OPC UA security support (authentication/encryption), we suggest using the openssl plugin for Open62541. For that to work, you need openssl on your system - both when compiling the client library and when generating any binaries (IOCs).
    On Linux, the name of the package you have to install depends on the distribution: development packages are called `openssl-devel` on RedHat/CentOS/Fedora and `libssl-dev` on Debian/Ubuntu, runtime packages are called `openssl` on RedHat/CentOS/Fedora and `libssl` on Debian/Ubuntu, respectively.
    See below for Windows instructions.
    In your `CONFIG_SITE.local` file (see below), enable the OPC UA Security option.

## Building Open62541

Note:
The Open62541 project is focused on the server implementation of OPC UA.
The client functionality is fully supported, complete and usable,
but it does not get the attention that the server parts get.

Do *not* use the download link on the open62541 web site.
Use their GitHub Release Page instead.

### On Linux

*   Unpack the open62541 distribution. Create a build directory on the top level and `cd` into it. We'll use the usual convention of calling it `build` .
    
*   The cmake build of Open62541 creates a static library by default. This type of library is needed for the EMBED type of Device Support build (see below).
    
    To create shared libraries, build the Open62541 library setting the cmake option `BUILD_SHARED_LIBS=ON`.
    
    ```Shell
    cmake .. -DBUILD_SHARED_LIBS=ON [...]
    ```
    
    You can create both types by running two builds with different configurations in the same build directory. Remove the CMakeCache in between to get clean configurations.
    
*   Open62541 supports multiple low-level libraries to implement OPC UA Security. The most widely used option (that also keeps things simple by staying in line with the UA SDK client) is based on openssl. We suggest to select that by setting the cmake option `UA_ENABLE_ENCRYPTION=OPENSSL`.

* Given all the above, for a Linux build, a reasonable option setting would be:

  ```shell
  cmake .. -DBUILD_SHARED_LIBS=ON \
           -DCMAKE_BUILD_TYPE=RelWithDebInfo \
           -DUA_ENABLE_ENCRYPTION=OPENSSL
  ```

  to create a shared library, and

  ```shell
  cmake .. -DBUILD_SHARED_LIBS=OFF \
           -DCMAKE_BUILD_TYPE=RelWithDebInfo \
           -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
           -DUA_ENABLE_ENCRYPTION=OPENSSL
  ```

  to create the static variant.

  There are a lot of other options available, but most of them affect only the server part and are not relevant for use of the client.

  For Open62541 version 1.2, set `UA_ENABLE_ENCRYPTION_OPENSSL=ON` instead of the OPENSSL setting shown above. You may also have to explicitly set the Open62541 version string in file `CMakeLists.txt`, as the auto-detection doesn't work when you're not working under git.

*   The default installation location is below `/usr/local`. Normally, this is a system location, so that the deploy mode setting (see below) would be `SYSTEM`.
    If you want to install into another location, set the option `CMAKE_INSTALL_PREFIX=/other/location`. In that case, the deploy mode setting (see below) would be `PROVIDED` and the library location needs to be set.
    The same is true
    if you want to use the Open62541 library
    in its build location (not recommended).
    The library location (set in `CONFIG_SITE.local`)
    will be `$(OPEN62541)/build/bin` or `$(OPEN62541)/build/bin/Release`
    or something different,
    depending on your installation, OS and choice of build directory name.
    
*   After running `cmake`, run `make` followed by `make install`. Installation into a system location will need root permission.

### On Windows

See the [Windows Installation How-To][windows-howto] for an overview and introduction to the different compilation options under Windows. 

#### Using MSYS2/MinGW

*   Install the necessary tools and dependencies using the `pacman` system package manager. You will need `mingw-w64-x86_64-cmake`, `mingw-w64-x86_64-libxml2` and `mingw-w64-x86_64-openssl`.

*   Unpack the open62541 distribution. Create a build directory on the top level and `cd` into it. We'll use the usual convention of calling it `build` .

*   A reasonable option setting for running `cmake` would look similar to the Linux variant:

    ```shell
    cmake .. -G "MinGW Makefiles" \
             -DBUILD_SHARED_LIBS=ON \
             -DCMAKE_BUILD_TYPE=RelWithDebInfo \
             -DUA_ENABLE_ENCRYPTION=OPENSSL
    ```

*   Run `mingw32-make` followed by `mingw32-make install`.

#### Using MSVC

*   Get the necessary prerequisites and install them:
    Python version 3 can be found [here][python].
    `libml2` and `iconv` binaries for Windows can be found [here][libxml2-mhils] or [here][libxml2-xmlsoft]. Install them in an appropriate location.
    You might have to rename `.dll.a` DLL export stubs to `.lib` to work properly with the EPICS build system.
    `openssl` can be found [here][openssl.slpro]. The installer makes sure that MSVC finds it (without explicit configuration).
    
* Unpack the open62541 distribution. Create a build directory on the top level and `cd` into it. We'll use the usual convention of calling it `build` .

* A reasonable option setting for running `cmake` would look like this:

  ```shell
  cmake .. -DBUILD_SHARED_LIBS=ON \
           -DCMAKE_BUILD_TYPE=RelWithDebInfo \
           -DUA_ENABLE_ENCRYPTION=OPENSSL
  ```

  plus `-DCMAKE_INSTALL_PREFIX=/other/location` if you want to select where things are installed.

  The cmake configuration seems to have a bug (seen on 1.3.6) that always selects `Release` for the path of the library to install. The easiest fix is to replace that string with the appropriate path (`Debug`) in two places in the block between lines 35 and 55 of the file `cmake_install.cmake`. 

* run `cmake --build .` followed by `cmake --install .` to build and install the library.

## Building the device support module

Inside the `configure` subdirectory or one level above the TOP location, create a file `RELEASE.local` that sets `EPICS_BASE` to the absolute path of your EPICS installation.

Inside the `configure` subdirectory or one level above the TOP location, create a file `CONFIG_SITE.local` that sets the absolute path of your Open62541 installation as well as its build and deploy features if necessary.
You also need to configure the locations of the other dependencies that you installed.

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

The Open62541 related configuration only has to be done in this module, which creates a `CONFIG_OPCUA` file that is automatically included by all downstream modules (those creating IOCs), so that the configuration is always consistent.

### On Windows

All paths in EPICS build configuration files must use Windows "short names" where needed, e.g.

```Makefile
OPEN62541 = C:/PROGRA~2/open62541-1.3.6-MinGW/
```

If you're using MSYS2/MinGW64, you need to set the location of the MinGW64 installation in a CONFIG_SITE file, e.g.

```makefile
# Location of MSYS2/MinGW64
MSYSTEM_PREFIX = C:/msys64/mingw64
```

When using MSVC you need to add the settings for the dependencies that you installed in non-system locations:

```makefile
# Prerequisites: libxml2 iconv
LIBXML2 = C:/dependency/location/libxml2-2.9.3
ICONV = C:/dependency/location/iconv-1.14
```

*Note:*
As always on Windows, the build environment as well as the runtime environment (for any IOC) need to have the locations of all DLLs in the PATH to be able to run binaries (which is part of the build for the unit tests). You will need to add the appropriate locations for EPICS Base, the opcua module, the open62541 library and all other dependencies you installed.
Alternatively, you can put copies of all needed DLLs into the location of the binaries. (Remember that `make distclean` will remove those copies.)

#### DLL Hell

Be aware that over time, Windows systems will have a large number of possibly different DLLs with the same names and/or versions being used concurrently on the system - a situation that is hard to maintain and known as [DLL Hell](https://en.wikipedia.org/wiki/DLL_Hell).

If you want your IOC binaries to be deployable
without depending on specific DLLs being present on the target system,
consider linking your IOCs statically.

## Feedback / Reporting issues

Please use the GitHub project's [issue tracker](https://github.com/epics-modules/opcua/issues).

## Known issues

n/a

## Credits

The adaption of the Open62541 client library has been done by Dirk Zimoch (PSI) with help from Carsten Winkler (HZB/BESSY II). Additional EPICS build system magic by Ralph Lange (ITER).

## License

This module is distributed subject to a Software License Agreement found
in file LICENSE that is included with this distribution.

<!-- Links -->

[windows-howto]: https://docs.epics-controls.org/projects/how-tos/en/latest/getting-started/installation-windows.html
[python]: https://www.python.org/downloads/windows/
[libxml2-mhils]: https://github.com/mhils/libxml2-win-binaries/releases
[libxml2-xmlsoft]: http://xmlsoft.org/sources/win32/64bit/
[openssl.slpro]: https://slproweb.com/products/Win32OpenSSL.html
