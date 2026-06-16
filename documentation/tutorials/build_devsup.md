(building-devsup)=
# Building the Device Support Module

This is a standard EPICS module.

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/epics-modules/opcua.git
    cd opcua
    ```

2.  **_(On Linux:)_ Configure the C++ Standard:**
    Configure the compiler on Linux to use the C++11 standard by adding
    ```makefile
    USR_CXXFLAGS_Linux += -std=c++11
    ```
    to the `CONFIG_SITE` file
    (or one of the host/target specific site configuration files).
    It is preferable to set this option globally in EPICS Base.

3.  **Configure the module:**
    Inside the `configure` subdirectory or one level above the TOP location,
    create a file `RELEASE.local`
    that sets `EPICS_BASE` (and optionally `GTEST`)
    to their absolute paths inside your EPICS installation.
    (The `GTEST` module is needed to compile and run the unit tests.
    Not defining it produces a clean build, but without any tests.)

    The configuration necessary
    when building against a specific client library
    is documented in the relevant section:
    [Configuring Device Support for open62541](devsup-conf-open62541)
    and [Configuring Device Support for UA SDK](devsup-conf-uasdk).

4.  **Build the module:**
    ```bash
    make
    ```
