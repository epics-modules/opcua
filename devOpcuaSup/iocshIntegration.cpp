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

namespace DevOpcua {

// Configurable defaults

double opcua_ConnectTimeout = 5.0;               // [s]
int opcua_MaxOperationsPerServiceCall = 0;       // no limit (do not batch)

double opcua_DefaultPublishInterval = 100.0;     // [ms]

double opcua_DefaultSamplingInterval = -1.0;     // use publish interval [ms]
int opcua_DefaultQueueSize = 1;                  // no queueing
int opcua_DefaultDiscardOldest = 1;              // discard oldest value in case of overrun
int opcua_DefaultUseServerTime = 1;              // use server timestamp
int opcua_DefaultOutputReadback = 1;             // make outputs bidirectional

extern "C" {
epicsExportAddress(double, opcua_ConnectTimeout);
epicsExportAddress(int, opcua_MaxOperationsPerServiceCall);
epicsExportAddress(double, opcua_DefaultPublishInterval);
epicsExportAddress(double, opcua_DefaultSamplingInterval);
epicsExportAddress(int, opcua_DefaultQueueSize);
epicsExportAddress(int, opcua_DefaultDiscardOldest);
epicsExportAddress(int, opcua_DefaultUseServerTime);
epicsExportAddress(int, opcua_DefaultOutputReadback);
}

} // namespace DevOpcua

namespace {

using namespace DevOpcua;

static const iocshArg opcuaCreateSessionArg0 = {"session name", iocshArgString};
static const iocshArg opcuaCreateSessionArg1 = {"server URL", iocshArgString};
static const iocshArg opcuaCreateSessionArg2 = {"debug level [0]", iocshArgInt};
static const iocshArg opcuaCreateSessionArg3 = {"autoconnect [true]", iocshArgString};

static const iocshArg *const opcuaCreateSessionArg[4] = {&opcuaCreateSessionArg0, &opcuaCreateSessionArg1,
                                                         &opcuaCreateSessionArg2, &opcuaCreateSessionArg3};

static const iocshFuncDef opcuaCreateSessionFuncDef = {"opcuaCreateSession", 4, opcuaCreateSessionArg};

static
void opcuaCreateSessionCallFunc (const iocshArgBuf *args)
{
    bool ok = true;
    std::string name;
    bool autoconnect = true;
    int debuglevel = 0;

    try {
        if (args[0].sval == NULL) {
            errlogPrintf("missing argument #1 (session name)\n");
            ok = false;
        } else if (strchr(args[0].sval, ' ')) {
            errlogPrintf("invalid argument #1 (session name) '%s'\n",
                         args[0].sval);
            ok = false;
        } else if (Session::sessionExists(args[0].sval)) {
            errlogPrintf("session name %s already in use\n",
                         args[0].sval);
            ok = false;
        }

        if (args[1].sval == NULL) {
            errlogPrintf("missing argument #2 (server URL)\n");
            ok = false;
        }

        if (args[2].ival < 0) {
            errlogPrintf("invalid argument #3 (debug level) '%d'\n",
                         args[2].ival);
        } else {
            debuglevel = args[2].ival;
        }

        if (args[3].sval != NULL) {
            char c = args[3].sval[0];
            if (c == 'n' || c == 'N' || c == 'f' || c == 'F') {
                autoconnect = false;
            } else if (c != 'y' && c != 'Y' && c != 't' && c != 'T') {
                errlogPrintf("invalid argument #4 (autoconnect) '%s'\n",
                             args[6].sval);
            }
        }

        if (ok) {
            Session::createSession(args[0].sval, args[1].sval, debuglevel, autoconnect);
            if (debuglevel)
                errlogPrintf("opcuaCreateSession: successfully created session '%s'\n", args[0].sval);
        } else {
            errlogPrintf("ERROR - no session created\n");
        }
    }
    catch(std::exception& e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaSetOptionArg0 = {"session name", iocshArgString};
static const iocshArg opcuaSetOptionArg1 = {"option name", iocshArgString};
static const iocshArg opcuaSetOptionArg2 = {"option value", iocshArgString};

static const iocshArg *const opcuaSetOptionArg[3] = {&opcuaSetOptionArg0, &opcuaSetOptionArg1,
                                                     &opcuaSetOptionArg2};

static const iocshFuncDef opcuaSetOptionFuncDef = {"opcuaSetOption", 3, opcuaSetOptionArg};

static
void opcuaSetOptionCallFunc (const iocshArgBuf *args)
{
    bool ok = true;
    bool help = false;

    try {
        if (args[0].sval == NULL) {
            errlogPrintf("missing argument #1 (session name)\n");
            ok = false;
        } else if (strcmp(args[0].sval, "help") == 0) {
            help = true;
        } else if (!Session::sessionExists(args[0].sval)) {
            errlogPrintf("'%s' - no such session\n",
                         args[0].sval);
            ok = false;
        }

        if (!help) {
            if (args[1].sval == NULL) {
                errlogPrintf("missing argument #2 (option name)\n");
                ok = false;
            } else if (strchr(args[1].sval, ' ')) {
                errlogPrintf("invalid argument #2 (option name) '%s'\n",
                             args[1].sval);
                ok = false;
            }

            if (args[2].sval == NULL) {
                if (strcmp(args[1].sval, "help") == 0) {
                    help = true;
                } else {
                    errlogPrintf("missing argument #3 (value)\n");
                    ok = false;
                }
            }
        }

        if (ok) {
            if (help) {
                Session::showOptionHelp();
            } else {
                Session &sess = Session::findSession(args[0].sval);
                sess.setOption(args[1].sval, args[2].sval);
            }
        } else {
            errlogPrintf("ERROR - no soption set\n");
        }
    }
    catch(std::exception& e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static
void opcuaIocshRegister ()
{
    iocshRegister(&opcuaCreateSessionFuncDef, opcuaCreateSessionCallFunc);
    iocshRegister(&opcuaSetOptionFuncDef, opcuaSetOptionCallFunc);
}

extern "C" {
epicsExportRegistrar(opcuaIocshRegister);
}

} // namespace
