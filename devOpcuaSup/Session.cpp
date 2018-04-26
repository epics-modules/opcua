/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 */

#include <iostream>
#include <map>
#include <exception>

#include <epicsExport.h>
#include <epicsExit.h>
#include <epicsThread.h>
#include <initHooks.h>
#include <errlog.h>

#include "Session.h"

namespace DevOpcua {

// Configurable defaults
// - connect timeout / reconnect attempt interval [sec]
// - batch size for operations (0 = no limit, don't batch)
static double opcua_ConnectTimeout = 5.0;
static int opcua_MaxOperationsPerServiceCall = 0;

extern "C" {
epicsExportAddress(double, opcua_ConnectTimeout);
epicsExportAddress(int, opcua_MaxOperationsPerServiceCall);
}

static epicsThreadOnceId session_ihooks_once = EPICS_THREAD_ONCE_INIT;
static epicsThreadOnceId session_atexit_once = EPICS_THREAD_ONCE_INIT;

std::map<std::string, Session*> Session::sessions;

static
void session_ihooks_register (void *junk)
{
    (void)junk;
    (void) initHookRegister(Session::initHook);
}

static
void session_atexit_register (void *junk)
{
    (void)junk;
    epicsAtExit(Session::atExit, NULL);
}

Session::Session (const std::string &name, const int debug, const bool autoConnect)
    : name(name)
    , debug(debug)
    , autoConnect(autoConnect)
{
    sessions[name] = this;
    epicsThreadOnce(&DevOpcua::session_ihooks_once, &DevOpcua::session_ihooks_register, NULL);
}

Session &
Session::findSession (const std::string &name)
{
    std::map<std::string, Session*>::iterator it = Session::sessions.find(name);
    if (it == Session::sessions.end()) {
        throw std::runtime_error("no such session");
    }
    return *(it->second);
}

bool
Session::sessionExists (const std::string &name)
{
    std::map<std::string, Session*>::iterator it = Session::sessions.find(name);
    return !(it == Session::sessions.end());
}

void
Session::showAll (int level)
{
    errlogPrintf("OPC UA: %lu session(s) configured\n", (long) sessions.size());
    std::map<std::string, Session*>::iterator it;
    for (it = sessions.begin(); it != sessions.end(); it++) {
        it->second->show(level);
    }
}

void
Session::setDebugAll (int level)
{
    std::map<std::string, Session*>::iterator it;
    for (it = sessions.begin(); it != sessions.end(); it++) {
        it->second->setDebug(level);
    }
}

void
Session::initHook (initHookState state)
{
    switch (state) {
    case initHookAfterDatabaseRunning:
    {
        std::map<std::string, Session*>::iterator it;
        errlogPrintf("OPC UA: Autoconnecting sessions\n");
        for (it = sessions.begin(); it != sessions.end(); it++) {
            if (it->second->autoConnect)
                it->second->connect();
        }
        epicsThreadOnce(&DevOpcua::session_atexit_once, &DevOpcua::session_atexit_register, NULL);
        break;
    }
    default:
        break;
    }
}

void
Session::atExit (void *junk)
{
    (void)junk;
    std::map<std::string, Session*>::iterator it;
    errlogPrintf("OPC UA: Disconnecting sessions\n");
    for (it = sessions.begin(); it != sessions.end(); it++) {
        it->second->disconnect();
    }
}

} // namespace DevOpcua
