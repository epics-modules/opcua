# Building the Low-Level OPC UA Client Library

Two options for the low-level client library are supported:

* the free open-source client of the
  [open62541 project SDK][open62541]
* the commercially available
  [Unified Automation C++ Based OPC UA Client SDK][uasdk].

These options are alternative - you only need one.

The choice is completely transparent on the IOC.
Neither your databases nor the startup script need any change
when you switch between the options.

Choose your option, then follow the appropriate instructions
that provide all the details:

* [**Building the open62541 client**](build-open62541)
* [**Building the Unified Automation client**](build-uasdk)

:::{include} option_a_open62541.markdown
:heading-offset: 1
:::

:::{include} option_b_uasdk.markdown
:heading-offset: 1
:::

<!-- Links -->
    
[open62541]: https://open62541.org/
[uasdk]: https://www.unified-automation.com/products/client-sdk/c-ua-client-sdk.html
