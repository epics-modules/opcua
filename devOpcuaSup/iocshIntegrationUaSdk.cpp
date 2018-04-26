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
#include <stdexcept>

#include <uaplatformlayer.h>

#include <iocsh.h>
#include <errlog.h>
#include <epicsExport.h>
#include <epicsThread.h>

#include "Session.h"
#include "SessionUaSdk.h"

namespace {

using namespace DevOpcua;

static epicsThreadOnceId opcuaUaSdk_once = EPICS_THREAD_ONCE_INIT;

static
void opcuaUaSdk_init (void *junk)
{
    (void)junk;
    UaPlatformLayer::init();
}

static const iocshArg opcuaCreateSessionUaSdkArg0 = {"session name", iocshArgString};
static const iocshArg opcuaCreateSessionUaSdkArg1 = {"server URL", iocshArgString};
static const iocshArg opcuaCreateSessionUaSdkArg2 = {"path to client certificate [none]", iocshArgString};
static const iocshArg opcuaCreateSessionUaSdkArg3 = {"path to client private key [none]", iocshArgString};
static const iocshArg opcuaCreateSessionUaSdkArg4 = {"debug level [0]", iocshArgInt};
static const iocshArg opcuaCreateSessionUaSdkArg5 = {"max nodes per service call [0=unlimited]", iocshArgInt};
static const iocshArg opcuaCreateSessionUaSdkArg6 = {"autoconnect [true]", iocshArgString};

static const iocshArg *const opcuaCreateSessionUaSdkArg[7] = {&opcuaCreateSessionUaSdkArg0, &opcuaCreateSessionUaSdkArg1,
                                                              &opcuaCreateSessionUaSdkArg2, &opcuaCreateSessionUaSdkArg3,
                                                              &opcuaCreateSessionUaSdkArg4, &opcuaCreateSessionUaSdkArg5,
                                                              &opcuaCreateSessionUaSdkArg6};

static const iocshFuncDef opcuaCreateSessionUaSdkFuncDef = {"opcuaCreateSessionUaSdk", 7, opcuaCreateSessionUaSdkArg};

static
void opcuaCreateSessionUaSdkCallFunc (const iocshArgBuf *args)
{
    bool ok = true;
    std::string name;
    bool autoconnect = true;
    int debuglevel = 0;
    int batchnodes = 0;

    try {
        epicsThreadOnce(&opcuaUaSdk_once, &opcuaUaSdk_init, NULL);

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

        if (args[4].ival < 0) {
            errlogPrintf("invalid argument #5 (debug level) '%d'\n",
                         args[4].ival);
        } else {
            debuglevel = args[4].ival;
        }

        if (args[5].ival < 0) {
            errlogPrintf("invalid argument #6 (max nodes per service call) '%d'\n",
                         args[5].ival);
        } else {
            batchnodes = args[5].ival;
        }

        if (args[6].sval != NULL) {
            char c = args[6].sval[0];
            if (c == 'n' || c == 'N' || c == 'f' || c == 'F') {
                autoconnect = false;
            } else if (c != 'y' && c != 'Y' && c != 't' && c != 'T') {
                errlogPrintf("invalid argument #7 (autoconnect) '%s'\n",
                             args[6].sval);
            }
        }

        if (ok) {
            new SessionUaSdk(args[0].sval, args[1].sval, autoconnect, debuglevel, batchnodes, args[2].sval, args[3].sval);
            if (debuglevel)
                errlogPrintf("opcuaCreateSessionUaSdk: successfully created session '%s'\n", args[0].sval);
        } else {
            errlogPrintf("ERROR - no session created\n");
        }
    }
    catch(std::exception& e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static
void opcuaUaSdkIocshRegister ()
{
    iocshRegister(&opcuaCreateSessionUaSdkFuncDef, opcuaCreateSessionUaSdkCallFunc);
}

extern "C" {
epicsExportRegistrar(opcuaUaSdkIocshRegister);
}

} // namespace
