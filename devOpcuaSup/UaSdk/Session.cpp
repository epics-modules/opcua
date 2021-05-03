/*************************************************************************\
* Copyright (c) 2018-2021 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 */

#include <iostream>

#include <uaplatformlayer.h>
#include <uabase.h>

#include <epicsThread.h>

#define epicsExportSharedSymbols
#include "Session.h"
#include "SessionUaSdk.h"
#include "Registry.h"

#define st(s) #s
#define str(s) st(s)

namespace DevOpcua {

static epicsThreadOnceId opcuaUaSdk_once = EPICS_THREAD_ONCE_INIT;

RegistryKeyNamespace RegistryKeyNamespace::global;

static
void opcuaUaSdk_init (void *junk)
{
    (void)junk;
    UaPlatformLayer::init();
}

Session *
Session::createSession(const std::string &name,
                       const std::string &url,
                       const int debuglevel,
                       const bool autoconnect)
{
    epicsThreadOnce(&opcuaUaSdk_once, &opcuaUaSdk_init, nullptr);
    if (RegistryKeyNamespace::global.contains(name))
        return nullptr;
    return new SessionUaSdk(name, url, autoconnect, debuglevel);
}

Session *
Session::find(const std::string &name)
{
    return SessionUaSdk::find(name);
}

std::set<Session *>
Session::glob(const std::string &pattern)
{
    return SessionUaSdk::glob(pattern);
}

void
Session::showAll (const int level)
{
    SessionUaSdk::showAll(level);
}

void
Session::showOptionHelp ()
{
    std::cout << "Options:\n"
              << "clientcert         path to client certificate [none]\n"
              << "clientkey          path to client private key [none]\n"
              << "nodes-max          max. nodes per service call [0 = no limit]\n"
              << "read-nodes-max     max. nodes per read service call [0 = no limit]\n"
              << "read-timeout-min   min. timeout (holdoff) after read service call [ms]\n"
              << "read-timeout-max   timeout (holdoff) after read service call w/ max elements [ms]\n"
              << "write-nodes-max    max. nodes per write service call [0 = no limit]\n"
              << "write-timeout-min  min. timeout (holdoff) after write service call [ms]\n"
              << "write-timeout-max  timeout (holdoff) after write service call w/ max elements [ms]"
              << std::endl;
}

const std::string &
opcuaGetDriverName ()
{
    static const std::string sdk("Unified Automation C++ Client SDK v"
                                 str(PROD_MAJOR) "." str(PROD_MINOR) "."
                                 str(PROD_PATCH) "-" str(PROD_BUILD));
    return sdk;
}

} // namespace DevOpcua
