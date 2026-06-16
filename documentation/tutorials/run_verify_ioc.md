# Running and Verifying Your IOC

## Use a GUI Client

Unified Automation offers a free professional GUI client application for OPC UA,
called **UAExpert**.
(Obviously, there are also other tools by other vendors.)
Using such a tool is highly recommended for any integration of OPC UA servers.

## Start your IOC

Run your IOC is the regular way.
```
cd iocBoot/iocUaDemoServer
./st.cmd
```

:::{hint}
Always exit your IOC gracefully using `^D` or `exit`.

Hard exits or killing the IOC process
will leave resources in use on the OPC UA server,
which will only be cleaned up after a grace period.
:::

## Enjoy OPC UA!

Change values (of the "Static" set) from UAExpert or the IOC -
via command line `pvget`/`pvput` -
and see them update/change on the other tool.

Browse the server from UAExpert to find more interesting variables,
then create records on the IOC to access them.
