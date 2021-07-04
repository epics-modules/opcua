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
#include <set>
#include <string>
#include <string.h>
#include <stdexcept>

#if defined(_WIN32)
#include <cstdlib>
#include <regex>
#endif

#include <iocsh.h>
#include <errlog.h>
#include <epicsThread.h>

#include <epicsExport.h>  // defines epicsExportSharedSymbols
#include "devOpcua.h"
#include "iocshVariables.h"
#include "linkParser.h"
#include "Session.h"
#include "Subscription.h"
#include "Registry.h"
#include "RecordConnector.h"

namespace DevOpcua {

// Configurable defaults

// session
double opcua_ConnectTimeout = 5.0;               // [s]
int opcua_MaxOperationsPerServiceCall = 0;       // no limit (do not batch)

// subscription
double opcua_DefaultPublishInterval = 100.0;     // [ms]

// monitored item
double opcua_DefaultSamplingInterval = -1.0;     // use publish interval [ms]
int opcua_DefaultServerQueueSize = 1;            // no queueing
int opcua_DefaultDiscardOldest = 1;              // discard oldest value in case of overrun
int opcua_DefaultUseServerTime = 1;              // use server timestamp
int opcua_DefaultOutputReadback = 1;             // make outputs bidirectional
double opcua_ClientQueueSizeFactor = 1.5;        // client queue size factor (* server side size)
int opcua_MinimumClientQueueSize = 3;            // minimum client queue size

extern "C" {
epicsExportAddress(double, opcua_ConnectTimeout);
epicsExportAddress(int, opcua_MaxOperationsPerServiceCall);
epicsExportAddress(double, opcua_DefaultPublishInterval);
epicsExportAddress(double, opcua_DefaultSamplingInterval);
epicsExportAddress(int, opcua_DefaultServerQueueSize);
epicsExportAddress(int, opcua_DefaultDiscardOldest);
epicsExportAddress(int, opcua_DefaultUseServerTime);
epicsExportAddress(int, opcua_DefaultOutputReadback);
epicsExportAddress(double, opcua_ClientQueueSizeFactor);
epicsExportAddress(int, opcua_MinimumClientQueueSize);
}

} // namespace DevOpcua

namespace {

using namespace DevOpcua;

// Create a std::string from a C string.
// On Windows: replacing any %varname%
//             with the value of the environment variable varname if it exists
static std::string
replaceEnvVars(const char *path)
{
    std::string result(path);
#if defined(_WIN32)
    std::string s(path);
    std::regex e("%([^ ]+)%"); // env var in Windows notation (e.g. %AppData%)
    std::smatch m;

    while (std::regex_search(s, m, e)) {
        char *v = std::getenv(m.str(1).c_str());
        if (v)
            result.replace(result.find(m.str(0)), m.str(0).length(), v);
        s = m.suffix().str();
    }
#endif
    return result;
}

static const iocshArg opcuaSessionArg0 = {"name", iocshArgString};
static const iocshArg opcuaSessionArg1 = {"URL", iocshArgString};
static const iocshArg opcuaSessionArg2 = {"[options]", iocshArgString};

static const iocshArg *const opcuaSessionArg[3] = {&opcuaSessionArg0,
                                                   &opcuaSessionArg1,
                                                   &opcuaSessionArg2};

const char opcuaSessionUsage[]
    = "Configures a new OPC UA session, assigning it a name and the URL "
      "of the OPC UA server.\nMust be called before iocInit.\n\n"
      "name       session name (no spaces)\n"
      "URL        URL of the OPC UA server (e.g. opc.tcp://192.168.1.23:4840)\n"
      "[options]  colon separated list of options in 'key=value' format\n"
      "           (see 'help opcuaOptions' for a list of valid options)\n";

static const iocshFuncDef opcuaSessionFuncDef = {"opcuaSession",
                                                 3,
                                                 opcuaSessionArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                 ,
                                                 opcuaSessionUsage
#endif
};

static void
opcuaSessionCallFunc(const iocshArgBuf *args)
{
    try {
        bool ok = true;
        Session *s = nullptr;
        int debug = 0;

        if (!args[0].sval) {
            errlogPrintf("missing argument #1 (session name)\n");
            ok = false;
        } else if (strchr(args[0].sval, ' ')) {
            errlogPrintf("invalid argument #1 (session name) '%s'\n", args[0].sval);
            ok = false;
        } else if (RegistryKeyNamespace::global.contains(args[0].sval)) {
            errlogPrintf("session name %s already in use\n", args[0].sval);
            ok = false;
        }

        if (!args[1].sval) {
            errlogPrintf("missing argument #2 (server URL)\n");
            ok = false;
        }

        std::list<std::pair<std::string, std::string>> setopts;
        if (args[2].sval) {
            auto options = splitString(args[2].sval, ':');
            for (auto &opt : options) {
                auto keyval = splitString(opt, '=');
                if (keyval.size() != 2) {
                    errlogPrintf("option '%s' must follow 'key=value' format - ignored\n",
                                 opt.c_str());
                } else {
                    if (keyval.front() == "debug")
                        debug = std::strtol(keyval.back().c_str(), nullptr, 0);
                    setopts.emplace_back(keyval.front(), keyval.back());
                }
            }
        }

        if (ok) {
            s = Session::createSession(args[0].sval, args[1].sval);
            if (s && debug)
                errlogPrintf("opcuaSession: successfully created session '%s'\n", args[0].sval);
        } else {
            errlogPrintf("ERROR - no session created\n");
            ok = false;
        }

        if (ok && s) {
            for (auto &keyval : setopts)
                s->setOption(keyval.first, keyval.second);
        }
    }
    catch(std::exception& e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaOptionsArg0 = {"pattern", iocshArgString};
static const iocshArg opcuaOptionsArg1 = {"[options]", iocshArgString};

static const iocshArg *const opcuaOptionsArg[2] = {&opcuaOptionsArg0, &opcuaOptionsArg1};

static const std::string opcuaOptionsUsage = std::string(Session::optionUsage) + Subscription::optionUsage;

static const iocshFuncDef opcuaOptionsFuncDef = {"opcuaOptions",
                                                2,
                                                opcuaOptionsArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                ,
                                                opcuaOptionsUsage.c_str()
#endif
};

static void
opcuaOptionsCallFunc(const iocshArgBuf *args)
{
    try {
        if (args[0].sval == NULL || args[0].sval[0] == '\0') {
            errlogPrintf("missing argument #1 (pattern for name)\n");
        } else if (strcmp(args[0].sval, "help") == 0) {
            std::cout << opcuaOptionsUsage.c_str() << std::endl;
        } else {
            if (!args[1].sval) {
                errlogPrintf("missing argument #2 (options)\n");
            } else {
                bool foundSomething = false;
                std::set<Session *> sessions = Session::glob(args[0].sval);
                if (sessions.size()) {
                    foundSomething = true;
                    auto options = splitString(args[1].sval, ':');
                    for (auto &opt : options) {
                        auto keyval = splitString(opt, '=');
                        if (keyval.size() != 2) {
                            errlogPrintf("option '%s' must follow 'key=value' format - ignored\n",
                                         opt.c_str());
                        } else {
                            for (auto &s : sessions)
                                s->setOption(keyval.front(), keyval.back());
                        }
                    }
                }
                if (!foundSomething) {
                    std::set<Subscription *> subscriptions = Subscription::glob(args[0].sval);
                    if (subscriptions.size()) {
                        foundSomething = true;
                        auto options = splitString(args[1].sval, ':');
                        for (auto &opt : options) {
                            auto keyval = splitString(opt, '=');
                            if (keyval.size() != 2) {
                                errlogPrintf(
                                    "option '%s' must follow 'key=value' format - ignored\n",
                                    opt.c_str());
                            } else {
                                for (auto &s : subscriptions)
                                    s->setOption(keyval.front(), keyval.back());
                            }
                        }
                    }
                }
                if (!foundSomething)
                    errlogPrintf("No matches for pattern '%s'\n", args[0].sval);
            }
        }
    } catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaMapNamespaceArg0 = {"session name", iocshArgString};
static const iocshArg opcuaMapNamespaceArg1 = {"namespace index", iocshArgInt};
static const iocshArg opcuaMapNamespaceArg2 = {"namespace URI", iocshArgString};

static const iocshArg *const opcuaMapNamespaceArg[3] = {&opcuaMapNamespaceArg0, &opcuaMapNamespaceArg1,
                                                        &opcuaMapNamespaceArg2};

static const iocshFuncDef opcuaMapNamespaceFuncDef = {"opcuaMapNamespace", 3, opcuaMapNamespaceArg};

static
void opcuaMapNamespaceCallFunc (const iocshArgBuf *args)
{
    try {
        bool ok = true;
        unsigned short index = 0;
        Session *s = nullptr;

        if (args[0].sval == nullptr) {
            errlogPrintf("missing argument #1 (session name)\n");
            ok = false;
        } else {
            s = Session::find(args[0].sval);
            if (!s) {
                errlogPrintf("'%s' - no such session\n", args[0].sval);
                ok = false;
            }
        }

        if (args[1].ival < 0 || args[1].ival > USHRT_MAX) {
            errlogPrintf("invalid argument #2 (namespace index) '%d'\n",
                         args[1].ival);
        } else {
            index = static_cast<unsigned short>(args[1].ival);
        }

        if (args[2].sval == nullptr) {
            errlogPrintf("missing argument #3 (namespace URI)\n");
            ok = false;
        }

        if (ok)
            s->addNamespaceMapping(index, args[2].sval);
    } catch (std::exception &e) {
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
    std::cerr << "DEPRECATION WARNING: opcuaShowSession is obsolete; use the improved opcuaShow "
                 "command instead (that supports glob patterns)."
              << std::endl;
    try {
        if (args[0].sval == nullptr || args[0].sval[0] == '\0') {
            Session::showAll(args[1].ival);
        } else {
            Session *s = Session::find(args[0].sval);
            if (s)
                s->show(args[1].ival);
        }
    } catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaShowSecurityArg0 = {"session name [\"\"=client]", iocshArgString};

static const iocshArg *const opcuaShowSecurityArg[1] = {&opcuaShowSecurityArg0};

static const iocshFuncDef opcuaShowSecurityFuncDef = {"opcuaShowSecurity", 1, opcuaShowSecurityArg};

static
void opcuaShowSecurityCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == nullptr || args[0].sval[0] == '\0') {
            Session::showClientSecurity();
        } else {
            Session *s = Session::find(args[0].sval);
            if (s)
                s->showSecurity();
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaClientCertificateArg0 = {"certificate (public key) file", iocshArgString};
static const iocshArg opcuaClientCertificateArg1 = {"private key file", iocshArgString};

static const iocshArg *const opcuaClientCertificateArg[2] = {&opcuaClientCertificateArg0, &opcuaClientCertificateArg1};

static const iocshFuncDef opcuaClientCertificateFuncDef = {"opcuaClientCertificate", 2, opcuaClientCertificateArg};

static
    void opcuaClientCertificateCallFunc (const iocshArgBuf *args)
{
    try {
        bool ok = true;

        if (args[0].sval == nullptr || args[0].sval[0] == '\0') {
            errlogPrintf("missing argument #1 (certificate file)\n");
            ok = false;
        }
        if (args[1].sval == nullptr || args[1].sval[0] == '\0') {
            errlogPrintf("missing argument #2 (private key file)\n");
            ok = false;
        }

        if (ok)
            Session::setClientCertificate(replaceEnvVars(args[0].sval),
                                          replaceEnvVars(args[1].sval));
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaSetupPKIArg0 = {"PKI / server certs location", iocshArgString};
static const iocshArg opcuaSetupPKIArg1 = {"server revocation lists location", iocshArgString};
static const iocshArg opcuaSetupPKIArg2 = {"issuer certs location", iocshArgString};
static const iocshArg opcuaSetupPKIArg3 = {"issuer revocation lists location", iocshArgString};

static const iocshArg *const opcuaSetupPKIArg[4] = {&opcuaSetupPKIArg0, &opcuaSetupPKIArg1,
                                                    &opcuaSetupPKIArg2, &opcuaSetupPKIArg3};

static const iocshFuncDef opcuaSetupPKIFuncDef = {"opcuaSetupPKI", 4, opcuaSetupPKIArg};

static
    void opcuaSetupPKICallFunc (const iocshArgBuf *args)
{
    try {
        // Special case: only one arg => points to PKI root, use default structure
        if (args[0].sval != nullptr && args[1].sval == nullptr) {
            std::string pki(replaceEnvVars(args[0].sval));
            if (pki.length() && pki.back() != pathsep)
                pki.push_back(pathsep);
            Session::setupPKI(pki + "trusted" + pathsep + "certs",
                              pki + "trusted" + pathsep + "crl",
                              pki + "issuers" + pathsep + "certs",
                              pki + "issuers" + pathsep + "crl");
        } else {
            bool ok = true;
            for (unsigned i = 0; i < 4; i++) {
                if (args[i].sval == nullptr || args[i].sval[0] == '\0') {
                    errlogPrintf("missing argument #%d - %s\n", i+1, opcuaSetupPKIArg[i]->name);
                    ok = false;
                }
            }
            if (ok) {
                Session::setupPKI(replaceEnvVars(args[0].sval),
                                  replaceEnvVars(args[1].sval),
                                  replaceEnvVars(args[2].sval),
                                  replaceEnvVars(args[3].sval));
            }
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaSaveRejectedArg0 = {"location for saving rejected certs", iocshArgString};

static const iocshArg *const opcuaSaveRejectedArg[1] = {&opcuaSaveRejectedArg0};

static const iocshFuncDef opcuaSaveRejectedFuncDef = {"opcuaSaveRejected", 1, opcuaSaveRejectedArg};

static
    void opcuaSaveRejectedCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == nullptr || args[0].sval[0] == '\0')
            Session::saveRejected();
        else
            Session::saveRejected(replaceEnvVars(args[0].sval));
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
            Session *s = Session::find(args[0].sval);
            if (s)
                s->connect(true);
        } catch (std::exception &e) {
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
            Session *s = Session::find(args[0].sval);
            if (s)
                s->disconnect();
        } catch (std::exception &e) {
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
        Session *s = Session::find(args[0].sval);
        if (s)
            s->debug = args[1].ival;
    } catch (std::exception &e) {
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
        } else if (Subscription::find(args[0].sval)) {
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
        } else if (!Session::find(args[1].sval)) {
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
    std::cerr
        << "DEPRECATION WARNING: opcuaShowSubscription is obsolete; use the improved opcuaShow "
           "command instead (that supports glob patterns)."
        << std::endl;
    try {
        if (args[0].sval == nullptr || args[0].sval[0] == '\0') {
            Subscription::showAll(args[1].ival);
        } else {
            Subscription *s = Subscription::find(args[0].sval);
            if (s)
                s->show(args[1].ival);
        }
    } catch (std::exception &e) {
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
        Subscription *s = Subscription::find(args[0].sval);
        if (s)
            s->debug = args[1].ival;
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaShowDataArg0 = {"record name", iocshArgString};
static const iocshArg opcuaShowDataArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowDataArg[2] = {&opcuaShowDataArg0, &opcuaShowDataArg1};

static const iocshFuncDef opcuaShowDataFuncDef = {"opcuaShowData", 2, opcuaShowDataArg};

static
void opcuaShowDataCallFunc (const iocshArgBuf *args)
{
    std::cerr << "DEPRECATION WARNING: opcuaShowData is obsolete; use the improved opcuaShow "
                 "command instead (that supports glob patterns)."
              << std::endl;
    try {
        if (args[0].sval == NULL || args[0].sval[0] == '\0') {
            errlogPrintf("missing argument #1 (record name)\n");
        } else {
            RecordConnector *rc = RecordConnector::findRecordConnector(args[0].sval);
            if (rc) {
                rc->pitem->show(args[1].ival);
            } else {
                errlogPrintf("record %s does not exist\n",
                             args[0].sval);
            }
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaShowArg0 = {"pattern", iocshArgString};
static const iocshArg opcuaShowArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowArg[2] = {&opcuaShowArg0, &opcuaShowArg1};

static const iocshFuncDef opcuaShowFuncDef = {"opcuaShow", 2, opcuaShowArg};

static void
opcuaShowCallFunc(const iocshArgBuf *args)
{
    if (args[0].sval == NULL || args[0].sval[0] == '\0') {
        errlogPrintf("missing argument #1 (pattern for name)\n");
    } else {
        bool foundSomething = false;
        std::set<Session *> sessions = Session::glob(args[0].sval);
        if (sessions.size()) {
            foundSomething = true;
            for (auto &s : sessions)
                s->show(args[1].ival);
        }
        if (!foundSomething) {
            std::set<Subscription *> subscriptions = Subscription::glob(args[0].sval);
            if (subscriptions.size()) {
                foundSomething = true;
                for (auto &s : subscriptions)
                    s->show(args[1].ival);
            }
        }
        if (!foundSomething) {
            std::set<RecordConnector *> connectors = RecordConnector::glob(args[0].sval);
            if (connectors.size()) {
                foundSomething = true;
                for (auto &rc : connectors)
                    rc->pitem->show(args[1].ival);
            }
        }
        if (!foundSomething)
            errlogPrintf("No matches for pattern '%s'\n", args[0].sval);
    }
}

// Deprecated functions (to be removed with v1.0)
// ----------------------------------------------

static const iocshArg opcuaCreateSessionArg0 = {"session name", iocshArgString};
static const iocshArg opcuaCreateSessionArg1 = {"server URL", iocshArgString};
static const iocshArg opcuaCreateSessionArg2 = {"debug level [0]", iocshArgInt};
static const iocshArg opcuaCreateSessionArg3 = {"autoconnect [true]", iocshArgString};

static const iocshArg *const opcuaCreateSessionArg[4] = {&opcuaCreateSessionArg0,
                                                         &opcuaCreateSessionArg1,
                                                         &opcuaCreateSessionArg2,
                                                         &opcuaCreateSessionArg3};

static const iocshFuncDef opcuaCreateSessionFuncDef = {"opcuaCreateSession",
                                                       4,
                                                       opcuaCreateSessionArg};

static void
opcuaCreateSessionCallFunc(const iocshArgBuf *args)
{
    std::cerr
        << "DEPRECATION WARNING: opcuaCreateSession is obsolete; use the improved opcuaSession "
           "command instead (that supports a generic option string)."
        << std::endl;

    try {
        bool ok = true;
        int debuglevel = 0;
        Session *s = nullptr;

        if (args[0].sval == nullptr) {
            errlogPrintf("missing argument #1 (session name)\n");
            ok = false;
        } else if (strchr(args[0].sval, ' ')) {
            errlogPrintf("invalid argument #1 (session name) '%s'\n", args[0].sval);
            ok = false;
        } else if (RegistryKeyNamespace::global.contains(args[0].sval)) {
            errlogPrintf("session name %s already in use\n", args[0].sval);
            ok = false;
        }

        if (args[1].sval == nullptr) {
            errlogPrintf("missing argument #2 (server URL)\n");
            ok = false;
        }

        if (args[2].ival < 0) {
            errlogPrintf("invalid argument #3 (debug level) '%d' - ignored\n", args[2].ival);
        } else {
            debuglevel = args[2].ival;
        }

        if (ok) {
            s = Session::createSession(args[0].sval, args[1].sval);
            if (debuglevel) {
                errlogPrintf("opcuaCreateSession: successfully created session '%s'\n",
                             args[0].sval);
                if (s)
                    s->setOption("debug", std::to_string(debuglevel));
            }
        } else {
            errlogPrintf("ERROR - no session created\n");
        }

        if (args[3].sval != nullptr) {
            if (s)
                s->setOption("autoconnect", args[3].sval);
        }

    } catch (std::exception &e) {
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
        Session *s = nullptr;

        if (args[0].sval == nullptr) {
            errlogPrintf("missing argument #1 (session name)\n");
            ok = false;
        } else if (strcmp(args[0].sval, "help") == 0) {
            help = true;
        } else {
            s = Session::find(args[0].sval);
            if (!s) {
                errlogPrintf("'%s' - no such session\n", args[0].sval);
                ok = false;
            }
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
                if (args[1].sval && strcmp(args[1].sval, "help") == 0) {
                    help = true;
                } else {
                    errlogPrintf("missing argument #3 (value)\n");
                    ok = false;
                }
            }
        }

        if (ok) {
            if (help)
                std::cout << opcuaOptionsUsage.c_str() << std::endl;
            else
                s->setOption(args[1].sval, args[2].sval);
        }
    } catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static
void opcuaIocshRegister ()
{
    iocshRegister(&opcuaSessionFuncDef, opcuaSessionCallFunc);
    iocshRegister(&opcuaOptionsFuncDef, opcuaOptionsCallFunc);
    iocshRegister(&opcuaMapNamespaceFuncDef, opcuaMapNamespaceCallFunc);

    iocshRegister(&opcuaConnectFuncDef, opcuaConnectCallFunc);
    iocshRegister(&opcuaDisconnectFuncDef, opcuaDisconnectCallFunc);
    iocshRegister(&opcuaShowSessionFuncDef, opcuaShowSessionCallFunc);
    iocshRegister(&opcuaShowSecurityFuncDef, opcuaShowSecurityCallFunc);
    iocshRegister(&opcuaClientCertificateFuncDef, opcuaClientCertificateCallFunc);
    iocshRegister(&opcuaSetupPKIFuncDef, opcuaSetupPKICallFunc);
    iocshRegister(&opcuaSaveRejectedFuncDef, opcuaSaveRejectedCallFunc);
    iocshRegister(&opcuaDebugSessionFuncDef, opcuaDebugSessionCallFunc);

    iocshRegister(&opcuaCreateSubscriptionFuncDef, opcuaCreateSubscriptionCallFunc);
    iocshRegister(&opcuaShowSubscriptionFuncDef, opcuaShowSubscriptionCallFunc);
    iocshRegister(&opcuaDebugSubscriptionFuncDef, opcuaDebugSubscriptionCallFunc);

    iocshRegister(&opcuaShowDataFuncDef, opcuaShowDataCallFunc);
    iocshRegister(&opcuaShowFuncDef, opcuaShowCallFunc);

    // Deprecated (to be removed at v1.0)
    iocshRegister(&opcuaCreateSessionFuncDef, opcuaCreateSessionCallFunc);
    iocshRegister(&opcuaSetOptionFuncDef, opcuaSetOptionCallFunc);

}

extern "C" {
epicsExportRegistrar(opcuaIocshRegister);
}

} // namespace
