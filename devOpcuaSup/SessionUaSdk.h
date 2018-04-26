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

#ifndef DEVOPCUA_SESSIONUASDK_H
#define DEVOPCUA_SESSIONUASDK_H

#include <uabase.h>
#include <uaclientsdk.h>

#include <epicsTypes.h>

#include "Session.h"

namespace DevOpcua {

using namespace UaClientSdk;

/**
 * @brief Session class for the Unified Automation OPC UA Client SDK.
 *
 * Main class for connecting with any OPC UA Server.
 * The class manages the connection to an OPC Unified Architecture server
 * and the application session established with the server.
 *
 * The connect call establishes and maintains a Session with a Server.
 * After a successful connect, the connection is monitored by the low level driver.
 * Connection status changes are reported through the callback
 * UaClientSdk::UaSessionCallback::connectionStatusChanged.
 *
 * The disconnect call disconnects the Session, deleting all Subscriptions
 * and freeing all related resources on both server and client.
 */

class SessionUaSdk : public UaSessionCallback, public Session
{
    UA_DISABLE_COPY(SessionUaSdk);

public:
    /**
     * @brief Create an OPC UA session.
     *
     * @param name               session name (used in EPICS record configuration)
     * @param serverUrl          OPC UA server URL
     * @param autoConnect        if true (default), client will automatically connect
     *                           both initially and after connection loss
     * @param debug              initial debug verbosity level
     * @param batchNodes         max. number of node to use in any single service call
     * @param clientCertificate  path to client-side certificate
     * @param clientPrivateKey   path to client-side private key
     */
    SessionUaSdk(const std::string &name, const std::string &serverUrl,
                 bool autoConnect=true, int debug=0, epicsUInt32 batchNodes=0,
                 const char *clientCertificate=NULL, const char *clientPrivateKey=NULL);
    ~SessionUaSdk();

    long connect();
    long disconnect();
    bool isConnected() const;

    void show(int level) const;

    // UaSessionCallback
    virtual void connectionStatusChanged(OpcUa_UInt32 clientConnectionId,
                                         UaClient::ServerStatus serverStatus);
private:
    static std::map<std::string, SessionUaSdk*> sessions; /**< session management */
    UaSession* pSession;
    SessionConnectInfo connectInfo;
    SessionSecurityInfo securityInfo;
    UaString serverURL;
    UaClient::ServerStatus serverConnectionStatus;
};

} // namespace DevOpcua

#endif // DEVOPCUA_SESSIONUASDK_H
