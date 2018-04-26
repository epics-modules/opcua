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
#include <string>
#include <string.h>
#include <stdexcept>

#include <iocsh.h>
#include <errlog.h>
#include <epicsExport.h>
#include <epicsThread.h>

#include "Session.h"

namespace {

using namespace DevOpcua;

static const iocshArg opcuaShowSessionArg0 = {"session name", iocshArgString};
static const iocshArg opcuaShowSessionArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowSessionArg[2] = {&opcuaShowSessionArg0, &opcuaShowSessionArg1};

static const iocshFuncDef opcuaShowSessionFuncDef = {"opcuaShowSession", 2, opcuaShowSessionArg};

static
void opcuaShowSessionCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == NULL) {
            Session::showAll(args[1].ival);
        } else {
            Session::findSession(args[0].sval).show(args[1].ival);
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaConnectArg0 = {"session name", iocshArgString};

static const iocshArg *const opcuaConnectArg[1] = {&opcuaConnectArg0};

static const iocshFuncDef opcuaConnectFuncDef = {"opcuaConnect", 1, opcuaConnectArg};

static
void opcuaConnectCallFunc (const iocshArgBuf *args)
{
    bool ok = true;

    if (args[0].sval == NULL) {
        errlogPrintf("ERROR : missing argument #1 (session name)\n");
        ok = false;
    }

    if (ok) {
        try {
            DevOpcua::Session &s = DevOpcua::Session::findSession(args[0].sval);
            s.connect();
        }
        catch (std::exception &e) {
            std::cerr << "ERROR : " << e.what() << std::endl;
        }
    }
}

static const iocshArg opcuaDisconnectArg0 = {"session name", iocshArgString};

static const iocshArg *const opcuaDisconnectArg[1] = {&opcuaDisconnectArg0};

static const iocshFuncDef opcuaDisconnectFuncDef = {"opcuaDisconnect", 1, opcuaDisconnectArg};

static
void opcuaDisconnectCallFunc (const iocshArgBuf *args)
{
    bool ok = true;

    if (args[0].sval == NULL) {
        errlogPrintf("ERROR : missing argument #1 (session name)\n");
        ok = false;
    }

    if (ok) {
        try {
            DevOpcua::Session &s = DevOpcua::Session::findSession(args[0].sval);
            s.disconnect();
        }
        catch (std::exception &e) {
            std::cerr << "ERROR : " << e.what() << std::endl;
        }
    }
}

static const iocshArg opcuaDebugSessionArg0 = {"session name [\"\"=all]", iocshArgString};
static const iocshArg opcuaDebugSessionArg1 = {"debug level [0]", iocshArgInt};

static const iocshArg *const opcuaDebugSessionArg[2] = {&opcuaDebugSessionArg0, &opcuaDebugSessionArg1};

static const iocshFuncDef opcuaDebugSessionFuncDef = {"opcuaDebugSession", 2, opcuaDebugSessionArg};

static
void opcuaDebugSessionCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == NULL || !strlen(args[0].sval)) {
            Session::setDebugAll(args[1].ival);
        } else {
            Session::findSession(args[0].sval).setDebug(args[1].ival);
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static
void opcuaIocshRegister ()
{
    iocshRegister(&opcuaConnectFuncDef, opcuaConnectCallFunc);
    iocshRegister(&opcuaDisconnectFuncDef, opcuaDisconnectCallFunc);
    iocshRegister(&opcuaShowSessionFuncDef, opcuaShowSessionCallFunc);
    iocshRegister(&opcuaDebugSessionFuncDef, opcuaDebugSessionCallFunc);
}

extern "C" {
epicsExportRegistrar(opcuaIocshRegister);
}

} // namespace
