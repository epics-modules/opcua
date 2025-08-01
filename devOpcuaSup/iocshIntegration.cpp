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
#include <winsock2.h>
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
static const iocshArg opcuaSessionArg2 = {"[options]", iocshArgArgv};

static const iocshArg *const opcuaSessionArg[3] = {&opcuaSessionArg0,
                                                   &opcuaSessionArg1,
                                                   &opcuaSessionArg2};

const char opcuaSessionUsage[]
    = "Configures a new OPC UA session, assigning it a name and the URL "
      "of the OPC UA server.\nMust be called before iocInit.\n\n"
      "name       session name (no spaces)\n"
      "URL        URL of the OPC UA server (e.g. opc.tcp://192.168.1.23:4840)\n"
      "[options]  list of options in 'key=value' format\n"
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
        for (int i = 1; i < args[2].aval.ac; i++) {
            auto options = splitString(args[2].aval.av[i], ':');
            for (auto &opt : options) {
                if (opt.empty()) continue;
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

static const iocshArg opcuaSubscriptionArg0 = {"name", iocshArgString};
static const iocshArg opcuaSubscriptionArg1 = {"session", iocshArgString};
static const iocshArg opcuaSubscriptionArg2 = {"publishing interval [ms]", iocshArgDouble};
static const iocshArg opcuaSubscriptionArg3 = {"[options]", iocshArgArgv};

static const iocshArg *const opcuaSubscriptionArg[4] = {&opcuaSubscriptionArg0,
                                                        &opcuaSubscriptionArg1,
                                                        &opcuaSubscriptionArg2,
                                                        &opcuaSubscriptionArg3};

const char opcuaSubscriptionUsage[]
    = "Configures a new OPC UA subscription, assigning it a name and creating it under an existing "
      "session.\nMust be called before iocInit.\n\n"
      "name                 subscription name (no spaces)\n"
      "session              name of the existing OPC UA session for the new subscription\n"
      "publishing interval  publishing interval for the new subscription (in ms)\n"
      "[options]            list of options in 'key=value' format\n"
      "                     (see 'help opcuaOptions' for a list of valid options)\n";

static const iocshFuncDef opcuaSubscriptionFuncDef = {"opcuaSubscription",
                                                      4,
                                                      opcuaSubscriptionArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                      ,
                                                      opcuaSubscriptionUsage
#endif
};

static
    void opcuaSubscriptionCallFunc (const iocshArgBuf *args)
{
    try {
        bool ok = true;
        double publishingInterval = 0.;
        int debug = 0;
        Subscription *s = nullptr;

        if (!args[0].sval) {
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

        if (!args[1].sval) {
            errlogPrintf("missing argument #2 (session name)\n");
            ok = false;
        } else if (!Session::find(args[1].sval)) {
            errlogPrintf("session %s does not exist\n",
                         args[1].sval);
            ok = false;
        }

        if (args[2].dval < 0) {
            errlogPrintf("invalid argument #3 (publishing interval) '%f' - ignored\n",
                         args[2].dval);
            publishingInterval = opcua_DefaultPublishInterval;
        } else if (args[2].dval == 0) {
            publishingInterval = opcua_DefaultPublishInterval;
        } else {
            publishingInterval = args[2].dval;
        }

        std::list<std::pair<std::string, std::string>> setopts;
        for (int i = 1; i < args[3].aval.ac; i++) {
            auto options = splitString(args[3].aval.av[i], ':');
            for (auto &opt : options) {
                if (opt.empty()) continue;
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
            s = Subscription::createSubscription(args[0].sval, args[1].sval, publishingInterval);
            if (s && debug)
                errlogPrintf("opcuaSubscription: successfully created subscription '%s'\n", args[0].sval);
        } else {
            errlogPrintf("ERROR - no subscription created\n");
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
static const iocshArg opcuaOptionsArg1 = {"[options]", iocshArgArgv};

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
            if (args[1].aval.ac <= 1) {
                errlogPrintf("missing argument #2 (options)\n");
            } else {
                bool foundSomething = false;
                std::set<Session *> sessions = Session::glob(args[0].sval);
                if (sessions.size()) {
                    foundSomething = true;
                    for (int i = 1; i < args[1].aval.ac; i++) {
                        auto options = splitString(args[1].aval.av[i], ':');
                        for (auto &opt : options) {
                            if (opt.empty()) continue;
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
                }
                if (!foundSomething) {
                    std::set<Subscription *> subscriptions = Subscription::glob(args[0].sval);
                    if (subscriptions.size()) {
                        foundSomething = true;
                        for (int i = 1; i < args[1].aval.ac; i++) {
                            auto options = splitString(args[1].aval.av[i], ':');
                            for (auto &opt : options) {
                                if (opt.empty()) continue;
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
                }
                if (!foundSomething)
                    errlogPrintf("No matches for pattern '%s'\n", args[0].sval);
            }
        }
    } catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaShowArg0 = {"pattern", iocshArgString};
static const iocshArg opcuaShowArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowArg[2] = {&opcuaShowArg0, &opcuaShowArg1};

const char opcuaShowUsage[]
    = "Prints information about sessions, subscriptions, items and their related data elements.\n\n"
      "pattern    glob pattern (supports * and ?) for session, subscription, record names\n"
      "verbosity  amount of printed information (default 0 = sparse)\n";

static const iocshFuncDef opcuaShowFuncDef = {"opcuaShow",
                                              2,
                                              opcuaShowArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                              ,
                                              opcuaShowUsage
#endif
};

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

static const iocshArg opcuaConnectArg0 = {"session", iocshArgString};

static const iocshArg *const opcuaConnectArg[1] = {&opcuaConnectArg0};

const char opcuaConnectUsage[]
    = "Attempts to connect sessions to the configured OPC UA server.\n"
      "For sessions with configured autoconnect option, the autoconnector is started.\n\n"
      "session  glob pattern (supports * and ?) for session names\n";

static const iocshFuncDef opcuaConnectFuncDef = {"opcuaConnect",
                                                 1,
                                                 opcuaConnectArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                 ,
                                                 opcuaConnectUsage
#endif
};

static
    void opcuaConnectCallFunc (const iocshArgBuf *args)
{
    bool ok = true;

    if (args[0].sval == nullptr) {
        errlogPrintf("ERROR : missing argument #1 (session name pattern)\n");
        ok = false;
    }

    if (ok) {
        try {
            std::set<Session *> sessions = Session::glob(args[0].sval);
            if (sessions.size())
                for (auto &s : sessions)
                    s->connect(true);
        } catch (std::exception &e) {
            std::cerr << "ERROR : " << e.what() << std::endl;
        }
    }
}

static const iocshArg opcuaDisconnectArg0 = {"session name", iocshArgString};

static const iocshArg *const opcuaDisconnectArg[1] = {&opcuaDisconnectArg0};

const char opcuaDisconnectUsage[]
    = "Gracefully disconnects sessions from the configured server.\n"
      "For sessions with configured autoconnect option, the autoconnector is stopped.\n\n"
      "session  glob pattern (supports * and ?) for session names\n";

static const iocshFuncDef opcuaDisconnectFuncDef = {"opcuaDisconnect",
                                                    1,
                                                    opcuaDisconnectArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                    ,
                                                    opcuaDisconnectUsage
#endif
};

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
            std::set<Session *> sessions = Session::glob(args[0].sval);
            if (sessions.size())
                for (auto &s : sessions)
                    s->disconnect();
        } catch (std::exception &e) {
            std::cerr << "ERROR : " << e.what() << std::endl;
        }
    }
}

static const iocshArg opcuaMapNamespaceArg0 = {"session", iocshArgString};
static const iocshArg opcuaMapNamespaceArg1 = {"namespace index", iocshArgInt};
static const iocshArg opcuaMapNamespaceArg2 = {"namespace URI", iocshArgString};

static const iocshArg *const opcuaMapNamespaceArg[3] = {&opcuaMapNamespaceArg0, &opcuaMapNamespaceArg1,
                                                        &opcuaMapNamespaceArg2};

const char opcuaMapNamespaceUsage[]
    = "Adds a namespace mapping to the mapping table of the specified session.\n"
      "The specified numerical namespace index (used in the loaded databases) will be mapped to "
      "the\nspecified namespace URI (on the server).\nThis allows to automatically adapt to servers "
      "that use volatile namespace indices.\n\n"
      "session          existing session name\n"
      "namespace index  numerical namespace as used in the database files\n"
      "namespace URI    full URI identification of that namespace\n";

static const iocshFuncDef opcuaMapNamespaceFuncDef = {"opcuaMapNamespace",
                                                      3,
                                                      opcuaMapNamespaceArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                      ,
                                                      opcuaMapNamespaceUsage
#endif
};

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

static const iocshArg opcuaShowSecurityArg0 = {"session name [\"\"=client]", iocshArgString};

static const iocshArg *const opcuaShowSecurityArg[1] = {&opcuaShowSecurityArg0};

const char opcuaShowSecurityUsage[]
    = "Prints information about the security setup of a sepcific session or the IOC client.\n\n"
      "session name  name of the session to report on (empty string for client report)\n";

static const iocshFuncDef opcuaShowSecurityFuncDef = {"opcuaShowSecurity",
                                                      1,
                                                      opcuaShowSecurityArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                      ,
                                                      opcuaShowSecurityUsage
#endif
};

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

const char opcuaClientCertificateUsage[]
    = "Sets up the OPC UA client certificates to use for the IOC client.\n\n"
      "certificate file  path to the file containing the certificate (public key)\n"
      "private key file  path to the file containing the private key\n";

static const iocshFuncDef opcuaClientCertificateFuncDef = {"opcuaClientCertificate", 2, opcuaClientCertificateArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                           ,
                                                           opcuaClientCertificateUsage
#endif
};

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

const char opcuaSetupPKIUsage[]
    = "Sets up the PKI file store of the IOC client, where certificates and revocation lists are "
      "stored.\n"
      "The first form (single parameter) expects a standard directory structure under the "
      "specified location.\n"
      "The second form (four parameters) explicitly defines the specific locations.\n\n"
      "PKI / server certs location       path to the PKI structure / to the location of "
      "trusted server certs\n"
      "server revocation lists location  path to the location of server revocation lists\n"
      "issuer certs location             path to the location of issuer certificates\n"
      "issuer revocation lists location  path to the location of issuer revocation lists\n";

static const iocshFuncDef opcuaSetupPKIFuncDef = {"opcuaSetupPKI",
                                                  4,
                                                  opcuaSetupPKIArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                  ,
                                                  opcuaSetupPKIUsage
#endif
};

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

const char opcuaSaveRejectedUsage[]
    = "Sets the location where the client will save rejected certificates.\n\n"
      "rejected certs location  where to save rejected certificates\n";

static const iocshFuncDef opcuaSaveRejectedFuncDef = {"opcuaSaveRejected",
                                                      1,
                                                      opcuaSaveRejectedArg
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                                      ,
                                                      opcuaSaveRejectedUsage
#endif
};

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
           "command instead (that supports a generic option list)."
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
    std::cerr << "DEPRECATION WARNING: opcuaCreateSubscription is obsolete; use the improved "
                 "opcuaSubscription command instead (that supports a generic option list)."
              << std::endl;

    try {
        bool ok = true;
        int debuglevel = 0;
        double publishingInterval = 0.;
        Subscription *s = nullptr;

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

        if (args[4].ival < 0) {
            errlogPrintf("out-of-range argument #5 (debug level) '%d' - ignored\n", args[4].ival);
        } else {
            debuglevel = args[4].ival;
        }

        if (ok) {
            s = Subscription::createSubscription(args[0].sval, args[1].sval, publishingInterval);
            if (s && debuglevel) {
                errlogPrintf("opcuaCreateSubscription: successfully configured subscription '%s'\n",
                             args[0].sval);
                s->setOption("debug", std::to_string(debuglevel));
            }
        } else {
            errlogPrintf("ERROR - no subscription created\n");
        }

        if (args[3].ival < 0 || args[3].ival > 255) {
            errlogPrintf("out-of-range argument #4 (priority) '%d' - ignored\n",
                         args[3].ival);
        } else {
            if (s)
                s->setOption("priority", std::to_string(args[3].ival));
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

static
void opcuaIocshRegister ()
{
    iocshRegister(&opcuaSessionFuncDef, opcuaSessionCallFunc);
    iocshRegister(&opcuaSubscriptionFuncDef, opcuaSubscriptionCallFunc);
    iocshRegister(&opcuaOptionsFuncDef, opcuaOptionsCallFunc);
    iocshRegister(&opcuaShowFuncDef, opcuaShowCallFunc);

    iocshRegister(&opcuaConnectFuncDef, opcuaConnectCallFunc);
    iocshRegister(&opcuaDisconnectFuncDef, opcuaDisconnectCallFunc);
    iocshRegister(&opcuaMapNamespaceFuncDef, opcuaMapNamespaceCallFunc);

    iocshRegister(&opcuaShowSecurityFuncDef, opcuaShowSecurityCallFunc);
    iocshRegister(&opcuaClientCertificateFuncDef, opcuaClientCertificateCallFunc);
    iocshRegister(&opcuaSetupPKIFuncDef, opcuaSetupPKICallFunc);
    iocshRegister(&opcuaSaveRejectedFuncDef, opcuaSaveRejectedCallFunc);

    // Deprecated (to be removed at v1.0)
    iocshRegister(&opcuaCreateSessionFuncDef, opcuaCreateSessionCallFunc);
    iocshRegister(&opcuaSetOptionFuncDef, opcuaSetOptionCallFunc);
    iocshRegister(&opcuaCreateSubscriptionFuncDef, opcuaCreateSubscriptionCallFunc);
    iocshRegister(&opcuaShowSessionFuncDef, opcuaShowSessionCallFunc);
    iocshRegister(&opcuaDebugSessionFuncDef, opcuaDebugSessionCallFunc);
    iocshRegister(&opcuaShowSubscriptionFuncDef, opcuaShowSubscriptionCallFunc);
    iocshRegister(&opcuaDebugSubscriptionFuncDef, opcuaDebugSubscriptionCallFunc);
    iocshRegister(&opcuaShowDataFuncDef, opcuaShowDataCallFunc);
}

extern "C" {
epicsExportRegistrar(opcuaIocshRegister);
}

} // namespace
