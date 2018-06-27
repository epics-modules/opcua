/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 *  and example code from the Unified Automation C++ Based OPC UA Client SDK
 */

//#include <string.h>
#include <iostream>
#include <string>
#include <map>

#include <uaclientsdk.h>
#include <uasession.h>

#include <epicsExport.h>
#include <epicsExit.h>
#include <epicsThread.h>
#include <initHooks.h>
#include <errlog.h>

#include "Session.h"
#include "SessionUaSdk.h"
#include "SubscriptionUaSdk.h"

namespace DevOpcua {

using namespace UaClientSdk;

static epicsThreadOnceId session_uasdk_ihooks_once = EPICS_THREAD_ONCE_INIT;
static epicsThreadOnceId session_uasdk_atexit_once = EPICS_THREAD_ONCE_INIT;

std::map<std::string, SessionUaSdk*> SessionUaSdk::sessions;

static
void session_uasdk_ihooks_register (void *junk)
{
    (void)junk;
    (void) initHookRegister(SessionUaSdk::initHook);
}

static
void session_uasdk_atexit_register (void *junk)
{
    (void)junk;
    epicsAtExit(SessionUaSdk::atExit, NULL);
}

inline const char *
serverStatusString (UaClient::ServerStatus type)
{
    switch (type) {
    case UaClient::Disconnected:                      return "Disconnected";
    case UaClient::Connected:                         return "Connected";
    case UaClient::ConnectionWarningWatchdogTimeout:  return "ConnectionWarningWatchdogTimeout";
    case UaClient::ConnectionErrorApiReconnect:       return "ConnectionErrorApiReconnect";
    case UaClient::ServerShutdown:                    return "ServerShutdown";
    case UaClient::NewSessionCreated:                 return "NewSessionCreated";
    default:                                          return "<unknown>";
    }
}

SessionUaSdk::SessionUaSdk (const std::string &name, const std::string &serverUrl,
                            bool autoConnect, int debug, epicsUInt32 batchNodes,
                            const char *clientCertificate, const char *clientPrivateKey)
    : name(name)
    , serverURL(serverUrl.c_str())
    , puasession(new UaSession())
    , serverConnectionStatus(UaClient::Disconnected)
{
    int status = -1;
    char host[256];

    status = gethostname(host, sizeof(host));
    if (status)
        strncpy(host, "unknown-host", sizeof(host));
    host[sizeof(host)-1] = '\0';

    //TODO: allow overriding by env variable
    connectInfo.sApplicationName = "EPICS IOC";
    connectInfo.sApplicationUri  = UaString("urn:%1:EPICS:IOC").arg(host);
    connectInfo.sProductUri      = "urn:EPICS:IOC";
    connectInfo.sSessionName     = UaString(name.c_str());

    connectInfo.bAutomaticReconnect = autoConnect;
    connectInfo.bRetryInitialConnect = autoConnect;
    connectInfo.nMaxOperationsPerServiceCall = batchNodes;

    //TODO: init security settings
    if ((clientCertificate && strlen(clientCertificate))
            || (clientPrivateKey && strlen(clientPrivateKey)))
        errlogPrintf("OPC UA security not supported yet\n");

    sessions[name] = this;
    epicsThreadOnce(&DevOpcua::session_uasdk_ihooks_once, &DevOpcua::session_uasdk_ihooks_register, NULL);
}

const std::string &
SessionUaSdk::getName() const
{
    return name;
}

SessionUaSdk &
SessionUaSdk::findSession (const std::string &name)
{
    std::map<std::string, SessionUaSdk*>::iterator it = sessions.find(name);
    if (it == sessions.end()) {
        throw std::runtime_error("no such session");
    }
    return *(it->second);
}

bool
SessionUaSdk::sessionExists (const std::string &name)
{
    std::map<std::string, SessionUaSdk*>::iterator it = sessions.find(name);
    return !(it == sessions.end());
}

//TODO: move Session::findSession() and Session::sessionExists()
// back to Session.cpp after adding implementation management there
Session &
Session::findSession (const std::string &name)
{
    return static_cast<Session &>(SessionUaSdk::findSession(name));
}

bool
Session::sessionExists (const std::string &name)
{
    return SessionUaSdk::sessionExists(name);
}

long
SessionUaSdk::connect ()
{
    if (!puasession) {
        std::cerr << "OPC UA session " << name.c_str()
                  << ": invalid session, cannot connect" << std::endl;
        return -1;
    }
    if (isConnected()) {
        if (debug) std::cerr << "OPC UA session " << name.c_str() << ": already connected ("
                             << serverStatusString(serverConnectionStatus) << ")" << std::endl;
        return 0;
    } else {
        UaStatus result = puasession->connect(serverURL, connectInfo, securityInfo, this);

        if (result.isGood()) {
            if (debug) std::cerr << "OPC UA session " << name.c_str()
                                 << ": connect service ok" << std::endl;
        } else {
            std::cerr << "OPC UA session " << name.c_str()
                      << ": connect service failed with status "
                      << result.toString().toUtf8() << std::endl;
        }
        // asynchronous: remaining actions are done at the status-change callback
        return !result.isGood();
    }
}

long
SessionUaSdk::disconnect ()
{
    if (isConnected()) {
        ServiceSettings serviceSettings;

        UaStatus result = puasession->disconnect(serviceSettings, OpcUa_True); // delete subscriptions

        if (result.isGood()) {
            if (debug) std::cerr << "OPC UA session " << name.c_str()
                                 << ": disconnect service ok" << std::endl;
        } else {
            std::cerr << "OPC UA session " << name.c_str()
                      << ": disconnect service failed with status "
                      << result.toString().toUtf8() << std::endl;
        }
        // Detach all subscriptions of this session from driver
        std::map<std::string, SubscriptionUaSdk*>::iterator it;
        for (it = subscriptions.begin(); it != subscriptions.end(); it++) {
            it->second->clear();
        }
        return !result.isGood();
    } else {
        if (debug) std::cerr << "OPC UA session " << name.c_str() << ": already disconnected ("
                             << serverStatusString(serverConnectionStatus) << ")" << std::endl;
        return 0;
    }
}

bool
SessionUaSdk::isConnected () const
{
    if (puasession)
        return !!puasession->isConnected();
    else
        return false;
}

void
SessionUaSdk::show (int level) const
{
    std::cerr << "session="      << name
              << " url="         << serverURL.toUtf8()
              << " status="      << serverStatusString(serverConnectionStatus)
              << " cert="        << "[none]"
              << " key="         << "[none]"
              << " debug="       << debug
              << " batch="       << puasession->maxOperationsPerServiceCall()
              << " autoconnect=" << (connectInfo.bAutomaticReconnect ? "Y" : "N")
              << std::endl;

    if (level >= 1) {
        std::map<std::string, SubscriptionUaSdk*>::const_iterator it;
        for (it = subscriptions.begin(); it != subscriptions.end(); it++) {
            it->second->show(level-1);
        }
    }
}

void SessionUaSdk::connectionStatusChanged(
    OpcUa_UInt32             clientConnectionId,
    UaClient::ServerStatus   serverStatus)
{
    OpcUa_ReferenceParameter(clientConnectionId);
    errlogPrintf("OPC UA session %s: Connection status changed from %s to %s\n",
                 name.c_str(),
                 serverStatusString(serverConnectionStatus),
                 serverStatusString(serverStatus));

    switch (serverStatus) {

        // "The monitoring of the connection to the server detected an error
        // and is trying to reconnect to the server."
    case UaClient::ConnectionErrorApiReconnect:
        break;
        // "The server sent a shut-down event and the client API tries a reconnect."
    case UaClient::ServerShutdown:
        break;
        // "The connection to the server is deactivated by the user of the client API."
    case UaClient::Disconnected:
        // TODO: set all records to invalid, and remove the OPC side type info
        break;

        // "The monitoring of the connection to the server indicated
        // a potential connection problem."
    case UaClient::ConnectionWarningWatchdogTimeout:
        break;

        // "The connection to the server is established and is working in normal mode."
    case UaClient::Connected:
        if (serverConnectionStatus == UaClient::ConnectionErrorApiReconnect
                || serverConnectionStatus == UaClient::NewSessionCreated
                || (serverConnectionStatus == UaClient::Disconnected)) {
            // TODO: register nodes, start subscriptions, add monitored items
        }
        break;

        // "The client was not able to reuse the old session
        // and created a new session during reconnect.
        // This requires to redo register nodes for the new session
        // or to read the namespace array."
    case UaClient::NewSessionCreated:
        break;
    }
    serverConnectionStatus = serverStatus;
}

void
SessionUaSdk::showAll (int level)
{
    std::cout << "OPC UA: "
              << sessions.size() << " session(s) configured"
              << std::endl;
    if (level >= 1) {
        std::map<std::string, SessionUaSdk*>::const_iterator it;
        for (it = sessions.begin(); it != sessions.end(); it++) {
            it->second->show(level-1);
        }
    }
}

SessionUaSdk::~SessionUaSdk ()
{
    if (puasession) {
        if (isConnected()) {
            ServiceSettings serviceSettings;
            puasession->disconnect(serviceSettings, OpcUa_True);
        }
        delete puasession;
    }
}

void
SessionUaSdk::initHook (initHookState state)
{
    switch (state) {
    case initHookAfterDatabaseRunning:
    {
        std::map<std::string, SessionUaSdk*>::iterator it;
        errlogPrintf("OPC UA: Autoconnecting sessions\n");
        for (it = sessions.begin(); it != sessions.end(); it++) {
            if (it->second->autoConnect)
                it->second->connect();
        }
        epicsThreadOnce(&DevOpcua::session_uasdk_atexit_once, &DevOpcua::session_uasdk_atexit_register, NULL);
        break;
    }
    default:
        break;
    }
}

void
SessionUaSdk::atExit (void *junk)
{
    (void)junk;
    std::map<std::string, SessionUaSdk*>::iterator it;
    errlogPrintf("OPC UA: Disconnecting sessions\n");
    for (it = sessions.begin(); it != sessions.end(); it++) {
        it->second->disconnect();
    }
}

} // namespace DevOpcua
