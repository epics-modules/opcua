/*************************************************************************\
* Copyright (c) 2018-2021 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#define epicsExportSharedSymbols

#include <iostream>
#include <epicsThread.h>
#include "Session.h"
#include "SessionOpen62541.h"
#include "Registry.h"

namespace DevOpcua {

RegistryKeyNamespace RegistryKeyNamespace::global;

Session *
Session::createSession(const std::string &name,
                       const std::string &url)
{
    if (RegistryKeyNamespace::global.contains(name))
        return nullptr;
    return new SessionOpen62541(name, url);
}

Session *
Session::find(const std::string &name)
{
    return SessionOpen62541::find(name);
}

void
Session::showAll (const int level)
{
    SessionOpen62541::showAll(level);
}

const char Session::optionUsage[]
    = "Sets options for existing OPC UA sessions or subscriptions.\n\n"
      "pattern    pattern for session or subscription names (* and ? supported)\n"
      "[options]  colon separated list of options in 'key=value' format\n\n"
      "Valid session options are:\n"
      "debug              debug level [default 0 = no debug]\n"
      "autoconnect        automatically connect sessions [default y]\n"
      "nodes-max          max. nodes per service call [0 = no limit]\n"
      "read-nodes-max     max. nodes per read service call [0 = no limit]\n"
      "read-timeout-min   min. timeout (holdoff) after read service call [ms]\n"
      "read-timeout-max   timeout (holdoff) after read service call w/ max elements [ms]\n"
      "write-nodes-max    max. nodes per write service call [0 = no limit]\n"
      "write-timeout-min  min. timeout (holdoff) after write service call [ms]\n"
      "write-timeout-max  timeout (holdoff) after write service call w/ max elements [ms]\n"
      "sec-mode           requested security mode\n"
      "sec-policy         requested security policy\n"
      "ident-file         file to read identity credentials from\n\n"
      "";

const std::string &
opcuaGetDriverName ()
{
    static const std::string version("Open62541 Client API " UA_OPEN62541_VER_COMMIT);
    return version;
}

} // namespace DevOpcua
