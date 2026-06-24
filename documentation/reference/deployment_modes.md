(deployment-modes)=
# Deployment Modes

The OPC UA Device Support module can be configured
to link and deploy the low-level OPC UA SDK
(either open62541 or Unified Automation UASDK)
using different strategies.
These strategies are referred to as **Deployment Modes**.

The deployment mode is controlled
by setting the `OPEN62541_DEPLOY_MODE` or `UASDK_DEPLOY_MODE` variable
in the `CONFIG_SITE.local` file of the Device Support module.

This only applies to shared (dynamic) linking, as static linking always
linkes all required Device Support and low-level functionality
directly into the IOC binary.

## Supported Modes

The following four modes are supported:

### **SYSTEM**

The shared libraries of the SDK
are expected to be in a system-standard location
(e.g., `/usr/lib` or `/usr/local/lib`).
The build system uses these libraries,
and they must be present on the target system at runtime.

This option is most appropriate when the SDK libraries can be installed
using the target system's package manager.

### **PROVIDED**

The shared libraries of the SDK
are located in a specific directory
defined by `OPEN62541_SHRLIB_DIR` or `UASDK_SHRLIB_DIR`.
These are used during the build,
and the target system's runtime environment
(e.g., `LD_LIBRARY_PATH` on Linux or `PATH` on Windows)
must be configured to find them.

This option is most appropriate when the SDK libraries are compiled from sources
and deployed to the same location on the target system.

### **INSTALL**

The shared libraries of the SDK are copied (installed)
into the Device Support module's own library/binary directory
during the build process.
This makes the module more self-contained
as it carries its own copies of the required SDK libraries.
Under Linux, this will also require
to configure the target system's runtime environment.

This option is most appropriate under Windows,
where keeping the DLLs next to the binary is the best way to ensure
that these will be the ones used when the binary is run.

### **EMBED**

The static library of the SDK is used to
directly link the required functions into the `opcua` shared library.
The SDK code becomes an integral part
of `libopcua.so` (on Linux) or `opcua.dll` (on Windows).
Consequently, the original SDK libraries are not required on the target system.

This option simplifies deployment (no low-level library needed on the target)
at the cost of possibly using more memory
(as other OPC UA clients cannot share the SDK library).

## Recommendations

The suggested Deployment Mode
depends on the operating system
and the desired linking strategy for the EPICS application.

### Windows

On Windows, the recommendation depends
on the linkage chosen for the EPICS application:

- **Safer Dynamic Linking:** `INSTALL` is preferred.
  This helps avoid "DLL hell"
  by ensuring that the necessary dependency libraries
  are copied to the application's binary directory.
- **Simplicity:** `EMBED` can be used for simplicity,
  particularly when the OPC UA Device Support
  is the only component on the system using that specific low-level library.

### Linux

On Linux,
the recommendation depends on how the low-level library is managed on the system:

- **System Packages:** Use `SYSTEM` when the low-level library
  has been installed using the system's package manager (such as `apt` or `yum`).
- **Custom Location:** Use `PROVIDED` when the low-level library 
  is deployed in a specific, non-standard location.
- **Simplicity:** `EMBED` can be used for simplicity,
  particularly when the OPC UA Device Support
  is the only component on the system using that specific low-level library.
