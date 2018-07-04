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

#include <algorithm>
#include <vector>

#include <uabase.h>
#include <uaclientsdk.h>

#include <epicsTypes.h>
#include <initHooks.h>

#include "Session.h"

namespace DevOpcua {

using namespace UaClientSdk;

class SubscriptionUaSdk;
class ItemUaSdk;

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
    friend class SubscriptionUaSdk;

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

    /**
     * @brief Print configuration and status of all sessions on stdout.
     *
     * The verbosity level controls the amount of information:
     * 0 = one summary
     * 1 = one line per session
     * 2 = one session line, then one line per subscription
     *
     * @param level  verbosity level
     */
    static void showAll(int level);

    /**
     * @brief Find a session by name.
     *
     * @param name session name
     *
     * @return SessionUaSdk & session
     */
    static SessionUaSdk & findSession(const std::string &name);

    /**
     * @brief Check if a session with the specified name exists.
     *
     * @param name  session name to search for
     *
     * @return bool
     */
    static bool sessionExists(const std::string &name);

    const std::string & getName() const;
    void show(int level) const;

    /**
     * @brief Add an item to the session.
     *
     * @param item  item to add
     */
    virtual void addItemUaSdk(ItemUaSdk *item);

    /**
     * @brief Remove an item from the session.
     *
     * @param item  item to remove
     */
    virtual void removeItemUaSdk(ItemUaSdk *item);

    /**
     * @brief EPICS IOC Database initHook function.
     *
     * Hook function called when the EPICS IOC is being initialized.
     * Connects all sessions with autoConnect=true.
     *
     * @param state  initialization state
     */
    static void initHook(initHookState state);

    /**
     * @brief EPICS IOC Database atExit function.
     *
     * Hook function called when the EPICS IOC is exiting.
     * Disconnects all sessions.
     */
    static void atExit(void *junk);

    // UaSessionCallback interface
    virtual void connectionStatusChanged(OpcUa_UInt32 clientConnectionId,
                                         UaClient::ServerStatus serverStatus);
private:
    static std::map<std::string, SessionUaSdk*> sessions;     /**< session management */

    const std::string name;                                   /**< unique session name */
    UaString serverURL;                                       /**< server URL */
    bool autoConnect;                                         /**< auto (re)connect flag */
    std::map<std::string, SubscriptionUaSdk*> subscriptions;  /**< subscriptions on this session */
    std::vector<ItemUaSdk *> items;                           /**< items on this session */
    UaSession* puasession;                                    /**< pointer to low level session */
    SessionConnectInfo connectInfo;                           /**< connection metadata */
    SessionSecurityInfo securityInfo;                         /**< security metadata */
    UaClient::ServerStatus serverConnectionStatus;            /**< connection status for this session */
};

} // namespace DevOpcua

#endif // DEVOPCUA_SESSIONUASDK_H
