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
#include <epicsThread.h>

#include <epicsExport.h>  // defines epicsExportSharedSymbols
#include "iocshVariables.h"
#include "Session.h"
#include "Subscription.h"

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
    try {
        bool ok = true;
        bool autoconnect = true;
        int debuglevel = 0;

        if (args[0].sval == nullptr) {
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

        if (args[1].sval == nullptr) {
            errlogPrintf("missing argument #2 (server URL)\n");
            ok = false;
        }

        if (args[2].ival < 0) {
            errlogPrintf("invalid argument #3 (debug level) '%d'\n",
                         args[2].ival);
        } else {
            debuglevel = args[2].ival;
        }

        if (args[3].sval != nullptr) {
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
    try {
        bool ok = true;
        bool help = false;

        if (args[0].sval == nullptr) {
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
            if (args[1].sval == nullptr) {
                errlogPrintf("missing argument #2 (option name)\n");
                ok = false;
            } else if (strchr(args[1].sval, ' ')) {
                errlogPrintf("invalid argument #2 (option name) '%s'\n",
                             args[1].sval);
                ok = false;
            }

            if (args[2].sval == nullptr) {
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

static const iocshArg opcuaShowSessionArg0 = {"session name", iocshArgString};
static const iocshArg opcuaShowSessionArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowSessionArg[2] = {&opcuaShowSessionArg0, &opcuaShowSessionArg1};

static const iocshFuncDef opcuaShowSessionFuncDef = {"opcuaShowSession", 2, opcuaShowSessionArg};

static
void opcuaShowSessionCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == nullptr || args[0].sval[0] == '\0') {
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

    if (args[0].sval == nullptr) {
        errlogPrintf("ERROR : missing argument #1 (session name)\n");
        ok = false;
    }

    if (ok) {
        try {
            DevOpcua::Session::findSession(args[0].sval).connect();
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

    if (args[0].sval == nullptr) {
        errlogPrintf("ERROR : missing argument #1 (session name)\n");
        ok = false;
    }

    if (ok) {
        try {
            DevOpcua::Session::findSession(args[0].sval).disconnect();
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
    try {
        bool ok = true;
        int debuglevel = 0;
        epicsUInt8 priority = 0;
        double publishingInterval = 0.;

        if (args[0].sval == nullptr) {
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

        if (args[1].sval == nullptr) {
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

        if (args[3].ival < 0 || args[3].ival > 255) {
            errlogPrintf("invalid argument #4 (priority) '%d'\n",
                         args[3].ival);
        } else {
            priority = static_cast<epicsUInt8>(args[3].ival);
        }

        if (args[4].ival < 0) {
            errlogPrintf("invalid argument #5 (debug level) '%d'\n",
                         args[4].ival);
        } else {
            debuglevel = args[4].ival;
        }

        if (ok) {
            Subscription::createSubscription(args[0].sval, args[1].sval,
                    publishingInterval, priority, debuglevel);
            if (debuglevel)
                errlogPrintf("opcuaCreateSubscription: successfully configured subscription '%s'\n", args[0].sval);
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
        if (args[0].sval == nullptr || args[0].sval[0] == '\0') {
            Subscription::showAll(args[1].ival);
        } else {
            Subscription::findSubscription(args[0].sval).show(args[1].ival);
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaDebugSubscriptionArg0 = {"subscription name [\"\"=all]", iocshArgString};
static const iocshArg opcuaDebugSubscriptionArg1 = {"debug level [0]", iocshArgInt};

static const iocshArg *const opcuaDebugSubscriptionArg[2] = {&opcuaDebugSubscriptionArg0, &opcuaDebugSubscriptionArg1};

static const iocshFuncDef opcuaDebugSubscriptionFuncDef = {"opcuaDebugSubscription", 2, opcuaDebugSubscriptionArg};

static
void opcuaDebugSubscriptionCallFunc (const iocshArgBuf *args)
{
    try {
        Subscription::findSubscription(args[0].sval).debug = args[1].ival;
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static
void opcuaIocshRegister ()
{
    iocshRegister(&opcuaCreateSessionFuncDef, opcuaCreateSessionCallFunc);
    iocshRegister(&opcuaSetOptionFuncDef, opcuaSetOptionCallFunc);

    iocshRegister(&opcuaConnectFuncDef, opcuaConnectCallFunc);
    iocshRegister(&opcuaDisconnectFuncDef, opcuaDisconnectCallFunc);
    iocshRegister(&opcuaShowSessionFuncDef, opcuaShowSessionCallFunc);
    iocshRegister(&opcuaDebugSessionFuncDef, opcuaDebugSessionCallFunc);

    iocshRegister(&opcuaCreateSubscriptionFuncDef, opcuaCreateSubscriptionCallFunc);
    iocshRegister(&opcuaShowSubscriptionFuncDef, opcuaShowSubscriptionCallFunc);
    iocshRegister(&opcuaDebugSubscriptionFuncDef, opcuaDebugSubscriptionCallFunc);
}

extern "C" {
epicsExportRegistrar(opcuaIocshRegister);
}

} // namespace
