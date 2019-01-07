# UA OPC UA ANSI C Demo Server

Unified Automation provides OPC UA Demo Servers [1], which are OPC UA ANSI C Demo Server and  OPC UA C++ Demo Server. These servers come with simulated Data, Events, Methods, and Historical Data for testing. 

## Enable the OPC UA Demo Server

Even if Servers run on Windows, we can setup them through Wine. Here is the following short instruction :

```
apt install wine
dpkg --add-architecture i386 && apt-get update && apt-get install wine32
wine uaserverc-win32-x86-vs2010sp1-v1.8.3-398.exe
```
The target path is defined as
```
${HOME}/.wine/drive_c/Program Files/UnifiedAutomation/UaAnsiCServer/bin/
```
There is a wrapper script to help to execute the server easily [2].

## The example databases
There are two namespaces (0 and 4) defined in UaOpcUaAnsiCDemoServer.template. 

* namespace 0 : few additional information on the demo server
* namesapce 4 : use the simple setup to get several demo variables


## Use a GUI to check

When developing, using a professional GUI client for OPC UA is strongly
suggested. The free UaExpert tool from Unified Automation is a good choice [3].


## Feedback / Reporting issues

Please use the OPC UA Device Support Module's GitHub
[issue tracker](https://github.com/ralphlange/opcua/issues).

## License

This example application is part of the OPC UA Device Support module
that is distributed subject to a Software License Agreement found
in file LICENSE that is included with its distribution.

## Reference

[1] https://www.unified-automation.com/downloads/opc-ua-servers.html
[2] https://github.com/jeonghanlee/uaopcua-server
[3] https://www.unified-automation.com/products/development-tools/uaexpert.html
