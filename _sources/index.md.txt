# EPICS OPC UA Device Support

[EPICS](https://epics-controls.org>) Device Support module
for interfacing to controllers and servers
using the
[OPC UA](https://opcfoundation.org/about/opc-technologies/opc-ua/) protocol.

- [Source Code](https://github.com/epics-modules/opcua) -
  everything you might want to know
- [Documentation](https://epics-modules.github.io/opcua) -
  this
- [OPC UA Room on EPICS Chat](https://matrix.to/#/#opcua:epics-controls.org) -
  quick help and discussions
- [Usage / FAQ (Wiki)](https://github.com/epics-modules/opcua/wiki) -
  hints & experiences with specific servers
- [Issue Tracker](https://github.com/epics-modules/opcua/issues>) -
  when things don't work

The Device Support sits on top of a low-level OPC UA client library.
Two SDKs are supported - you need to choose and provide one of them:

* The freely available client library in the
  [SDK of the open62541 project][open62541]

* The commercially available
  [Unified Automation C++ Based OPC UA Client SDK][uasdk].

Linux and Windows builds are supported for both client library options.

## Dependencies

* C++11 compliant compiler
* [EPICS Base][epics-base]
  3.15 (>= 3.15.7) or EPICS 7 (>= 7.0.4)
* One of the two supported SDKs (see above)
* *(Optional)* The [gtest module][epics-gtest]
  if you want to compile and run the unit tests

See [building](building-devsup) for details.

## Download

Releases are published
on the [GitHub release page][opcua-releases]

This includes binary distribution tars,
which allow using the commercial Unified Automation Client SDK libraries
without any purchase or fee.

```{toctree}
:maxdepth: 2
:caption: Tutorials
:glob:

tutorials/??_*
```

```{toctree}
:maxdepth: 2
:caption: How-to Guides
:glob:

how-to/*
```

```{toctree}
:maxdepth: 2
:caption: Reference
:glob:

reference/*
```

```{toctree}
:maxdepth: 2
:caption: Explanation
:glob:

explanation/*
```

```{toctree}
:maxdepth: 2
:caption: Miscellanea

credits.md
license.md
```

Indices and tables
==================

* {ref}`genindex`
* {ref}`search`

<!-- Links -->
[open62541]: https://open62541.org/
[uasdk]: https://www.unified-automation.com/products/client-sdk/c-ua-client-sdk.html
[epics-base]: https://epics-controls.org/resources-and-support/base/
[epics-gtest]: https://github.com/epics-modules/gtest/
[opcua-releases]: https://github.com/epics-modules/opcua/releases/
