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
#include "SubscriptionUaSdk.h"

namespace DevOpcua {

// Configurable defaults
// - publishing interval [ms]

static double opcua_DefaultPublishInterval = 100.0;  // ms

extern "C" {
epicsExportAddress(double, opcua_DefaultPublishInterval);
}

} // namespace DevOpcua

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

static const iocshArg opcuaShowSessionArg0 = {"session name", iocshArgString};
static const iocshArg opcuaShowSessionArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowSessionArg[2] = {&opcuaShowSessionArg0, &opcuaShowSessionArg1};

static const iocshFuncDef opcuaShowSessionFuncDef = {"opcuaShowSession", 2, opcuaShowSessionArg};

static
void opcuaShowSessionCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == NULL || args[0].sval[0] == '\0') {
            SessionUaSdk::showAll(args[1].ival);
        } else {
            SessionUaSdk::findSession(args[0].sval).show(args[1].ival);
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
        Session::findSession(args[0].sval).debug = args[1].ival;
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaCreateSubscriptionArg0 = {"subscription name", iocshArgString};
static const iocshArg opcuaCreateSubscriptionArg1 = {"session name", iocshArgString};
static const iocshArg opcuaCreateSubscriptionArg2 = {"publishing interval (ms)", iocshArgDouble};
static const iocshArg opcuaCreateSubscriptionArg3 = {"priority [0]", iocshArgInt};
static const iocshArg opcuaCreateSubscriptionArg4 = {"debug level [0]", iocshArgInt};

static const iocshArg *const opcuaCreateSubscriptionArg[5] = {&opcuaCreateSubscriptionArg0, &opcuaCreateSubscriptionArg1,
                                                              &opcuaCreateSubscriptionArg2, &opcuaCreateSubscriptionArg3,
                                                              &opcuaCreateSubscriptionArg4};

static const iocshFuncDef opcuaCreateSubscriptionFuncDef = {"opcuaCreateSubscription", 5, opcuaCreateSubscriptionArg};

static
void opcuaCreateSubscriptionCallFunc (const iocshArgBuf *args)
{
    bool ok = true;
    std::string name;
    int debuglevel = 0;
    int priority = 0;
    double publishingInterval = 0.;

    try {
        epicsThreadOnce(&opcuaUaSdk_once, &opcuaUaSdk_init, NULL);

        if (args[0].sval == NULL) {
            errlogPrintf("missing argument #1 (subscription name)\n");
            ok = false;
        } else if (strchr(args[0].sval, ' ')) {
            errlogPrintf("invalid argument #1 (subscription name) '%s'\n",
                         args[0].sval);
            ok = false;
        } else if (Subscription::subscriptionExists(args[0].sval)) {
            errlogPrintf("subscription name %s already in use\n",
                         args[0].sval);
            ok = false;
        }

        if (args[1].sval == NULL) {
            errlogPrintf("missing argument #2 (session name)\n");
            ok = false;
        } else if (strchr(args[1].sval, ' ')) {
            errlogPrintf("invalid argument #2 (session name) '%s'\n",
                         args[1].sval);
            ok = false;
        } else if (!Session::sessionExists(args[1].sval)) {
            errlogPrintf("session %s does not exist\n",
                         args[1].sval);
            ok = false;
        }

        if (args[2].dval < 0) {
            errlogPrintf("invalid argument #3 (publishing interval) '%f'\n",
                         args[2].dval);
            ok = false;
        } else if (args[2].dval == 0) {
            publishingInterval = opcua_DefaultPublishInterval;
        } else {
            publishingInterval = args[2].dval;
        }

        if (args[3].ival < 0) {
            errlogPrintf("invalid argument #4 (priority) '%d'\n",
                         args[3].ival);
        } else {
            priority = args[3].ival;
        }

        if (args[4].ival < 0) {
            errlogPrintf("invalid argument #5 (debug level) '%d'\n",
                         args[4].ival);
        } else {
            debuglevel = args[4].ival;
        }

        if (ok) {
            new SubscriptionUaSdk(args[0].sval, &SessionUaSdk::findSession(args[1].sval),
                    publishingInterval, priority, debuglevel);
            if (debuglevel)
                errlogPrintf("opcuaCreateSubscriptionUaSdk: successfully configured subscription '%s'\n", args[0].sval);
        } else {
            errlogPrintf("ERROR - no subscription created\n");
        }
    }
    catch(std::exception& e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaShowSubscriptionArg0 = {"subscription name", iocshArgString};
static const iocshArg opcuaShowSubscriptionArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowSubscriptionArg[2] = {&opcuaShowSubscriptionArg0, &opcuaShowSubscriptionArg1};

static const iocshFuncDef opcuaShowSubscriptionFuncDef = {"opcuaShowSubscription", 2, opcuaShowSubscriptionArg};

static
void opcuaShowSubscriptionCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == NULL || args[0].sval[0] == '\0') {
            SubscriptionUaSdk::showAll(args[1].ival);
        } else {
            SubscriptionUaSdk::findSubscription(args[0].sval).show(args[1].ival);
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static
void opcuaUaSdkIocshRegister ()
{
    iocshRegister(&opcuaCreateSessionUaSdkFuncDef, opcuaCreateSessionUaSdkCallFunc);
    iocshRegister(&opcuaConnectFuncDef, opcuaConnectCallFunc);
    iocshRegister(&opcuaDisconnectFuncDef, opcuaDisconnectCallFunc);
    iocshRegister(&opcuaShowSessionFuncDef, opcuaShowSessionCallFunc);
    iocshRegister(&opcuaDebugSessionFuncDef, opcuaDebugSessionCallFunc);

    iocshRegister(&opcuaCreateSubscriptionFuncDef, opcuaCreateSubscriptionCallFunc);
    iocshRegister(&opcuaShowSubscriptionFuncDef, opcuaShowSubscriptionCallFunc);
}

extern "C" {
epicsExportRegistrar(opcuaUaSdkIocshRegister);
}

} // namespace
