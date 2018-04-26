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

#include <string.h>
#include <iostream>

#include <uaclientsdk.h>
#include <uasession.h>

#include <epicsTypes.h>
#include <epicsThread.h>
#include <errlog.h>

#include "SessionUaSdk.h"

namespace DevOpcua {

using namespace UaClientSdk;

std::map<std::string, SessionUaSdk*> SessionUaSdk::sessions;

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
    : Session(name, debug, autoConnect)
    , pSession(new UaSession())
    , serverURL(serverUrl.c_str())
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
}

long
SessionUaSdk::connect ()
{
    if (!pSession) {
        std::cerr << "OPC UA session " << name.c_str()
                  << ": invalid session, cannot connect" << std::endl;
        return -1;
    }
    if (isConnected()) {
        if (debug) std::cerr << "OPC UA session " << name.c_str() << ": already connected ("
                             << serverStatusString(serverConnectionStatus) << ")" << std::endl;
        return 0;
    } else {
        UaStatus result = pSession->connect(serverURL, connectInfo, securityInfo, this);

        if (result.isGood()) {
            if (debug) std::cerr << "OPC UA session " << name.c_str()
                                 << ": connect service ok" << std::endl;
        } else {
            std::cerr << "OPC UA session " << name.c_str()
                      << ": connect service failed with status "
                      << result.toString().toUtf8() << std::endl;
        }
        return !result.isGood();
    }
}

long
SessionUaSdk::disconnect ()
{
    if (isConnected()) {
        ServiceSettings serviceSettings;

        UaStatus result = pSession->disconnect(serviceSettings, OpcUa_True); // delete subscriptions

        if (result.isGood()) {
            if (debug) std::cerr << "OPC UA session " << name.c_str()
                                 << ": disconnect service ok" << std::endl;
        } else {
            std::cerr << "OPC UA session " << name.c_str()
                      << ": disconnect service failed with status "
                      << result.toString().toUtf8() << std::endl;
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
    if (pSession)
        return !!pSession->isConnected();
    else
        return false;
}

void
SessionUaSdk::show (int level) const
{
    errlogPrintf("name=%s url=%s cert=%s key=%s debug=%d batch=%u autoconnect=%c state=%s\n",
                 name.c_str(), serverURL.toUtf8(), "[unsupported]", "[unsupported]",
                 debug, connectInfo.nMaxOperationsPerServiceCall,
                 connectInfo.bAutomaticReconnect ? 'Y' : 'N',
                 serverStatusString(serverConnectionStatus));
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
        // "The server sent a shut-down event and the client API tries a reconnect."
    case UaClient::ServerShutdown:
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

SessionUaSdk::~SessionUaSdk ()
{
    if (pSession) {
        if (isConnected()) {
            ServiceSettings serviceSettings;
            pSession->disconnect(serviceSettings, OpcUa_True);
        }
        delete pSession;
    }
}

} // namespace DevOpcua
