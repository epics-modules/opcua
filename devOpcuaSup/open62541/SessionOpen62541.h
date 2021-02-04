/*************************************************************************\
* Copyright (c) 2018-2019 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#ifndef DEVOPCUA_SESSIONOPEN62541_H
#define DEVOPCUA_SESSIONOPEN62541_H

#include <algorithm>
#include <vector>
#include <memory>
#include <map>

#include <epicsMutex.h>
#include <epicsTypes.h>
#include <epicsTimer.h>
#include <initHooks.h>

#include <open62541/client_config_default.h>

#include "RequestQueueBatcher.h"
#include "Session.h"

namespace DevOpcua {

class SubscriptionOpen62541;
class ItemOpen62541;
struct WriteRequest;
struct ReadRequest;

// some helpers
inline const char* toStr(const UA_String& ua_string) {
    return reinterpret_cast<char*>(ua_string.data);
}

inline std::ostream& operator<<(std::ostream& os, const UA_String& ua_string) {
    return os << ua_string.data;
}

inline std::ostream& operator<<(std::ostream& os, const UA_NodeId& ua_nodeId) {
    UA_String s;
    UA_String_init(&s);
    UA_NodeId_print(&ua_nodeId, &s);
    os << s.data;
    UA_String_clear(&s);
    return os;
}

/**
 * @brief The SessionOpen62541 implementation of an open62541 client session.
 *
 * The connect call establishes and maintains a Session with a Server.
 * After a successful connect, the connection is monitored by the low level driver.
 * Connection status changes are reported through the connectionStatusChanged callback.
 *
 * The disconnect call disconnects the Session, deleting all Subscriptions
 * and freeing all related resources on both server and client.
 */

class SessionOpen62541
        : public Session
        , public RequestConsumer<WriteRequest>
        , public RequestConsumer<ReadRequest>
        , public epicsTimerNotify
{
    // Cannot copy a Session
    SessionOpen62541(const SessionOpen62541 &);
    SessionOpen62541 &operator=(const SessionOpen62541 &);

    friend class SubscriptionOpen62541;

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
    SessionOpen62541(const std::string &name, const std::string &serverUrl,
                 bool autoConnect = true, int debug = 0, epicsUInt32 batchNodes = 0,
                 const char *clientCertificate = nullptr, const char *clientPrivateKey = nullptr);
 private:
    ~SessionOpen62541() override;

    /**
     * @brief Connect session. See DevOpcua::Session::connect
     * @return long status (0 = OK)
     */
    virtual long connect() override;
    /**
     * @brief Disconnect session. See DevOpcua::Session::disconnect
     * @return long status (0 = OK)
     */
    virtual long disconnect() override;

    /**
     * @brief Return connection status. See DevOpcua::Session::isConnected
     * @return
     */
    virtual bool isConnected() const override;

    /**
     * @brief Print configuration and status. See DevOpcua::Session::show
     * @param level
     */
    virtual void show(const int level) const override;
 public:

    /**
     * @brief Get session name. See DevOpcua::Session::getName
     * @return session name
     */
    virtual const std::string & getName() const override;

    /**
     * @brief Get a structure definition from the session dictionary.
     * @param dataTypeId data type of the extension object
     * @return structure definition
     */
//    UaStructureDefinition structureDefinition(const UaNodeId &dataTypeId)
//    { return puasession->structureDefinition(dataTypeId); }

    /**
     * @brief Request a beginRead service for an item
     *
     * @param item  item to request beginRead for
     */
    void requestRead(ItemOpen62541 &item);

    /**
     * @brief Request a beginWrite service for an item
     *
     * @param item  item to request beginWrite for
     */
    void requestWrite(ItemOpen62541 &item);

 private:
    /**
     * @brief Create all subscriptions related to this session.
     */
    void createAllSubscriptions();

    /**
     * @brief Add all monitored items to subscriptions related to this session.
     */
    void addAllMonitoredItems();

 public:
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
    static void showAll(const int level);

    /**
     * @brief Find a session by name.
     *
     * @param name session name
     *
     * @return SessionOpen62541 & session
     */
    static SessionOpen62541 & findSession(const std::string &name);

    /**
     * @brief Check if a session with the specified name exists.
     *
     * @param name  session name to search for
     *
     * @return bool
     */
    static bool sessionExists(const std::string &name);

 private:
    /**
     * @brief Set an option for the session. See DevOpcua::Session::setOption
     */
    virtual void setOption(const std::string &name, const std::string &value) override;

    /**
     * @brief Add namespace index mapping (local). See DevOpcua::Session::addNamespaceMapping
     */
    virtual void addNamespaceMapping(const unsigned short nsIndex, const std::string &uri) override;

    unsigned int noOfSubscriptions() const { return static_cast<unsigned int>(subscriptions.size()); }
    unsigned int noOfItems() const { return static_cast<unsigned int>(items.size()); }

 public:
    /**
     * @brief Add an item to the session.
     *
     * @param item  item to add
     */
    void addItemOpen62541(ItemOpen62541 *item);

    /**
     * @brief Remove an item from the session.
     *
     * @param item  item to remove
     */
    void removeItemOpen62541(ItemOpen62541 *item);

    /**
     * @brief Map namespace index (local -> server)
     *
     * @param nsIndex  local namespace index
     *
     * @return server-side namespace index
     */
    UA_UInt16 mapNamespaceIndex(const UA_UInt16 nsIndex) const;

 private:
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

    // Get a new (unique per session) transaction id
    UA_UInt32 getTransactionId();

/*
    // UaSessionCallback interface

    virtual void readComplete(
            UA_UInt32 transactionId,
            const UA_StatusCode result,
            const UaDataValues &values,
            const UaDiagnosticInfos &diagnosticInfos) override;

    virtual void writeComplete(
            UA_UInt32 transactionId,
            const UA_StatusCode result,
            const UaStatusCodeArray &results,
            const UaDiagnosticInfos &diagnosticInfos) override;
*/

    // RequestConsumer<> interfaces
    virtual void processRequests(std::vector<std::shared_ptr<WriteRequest>> &batch) override;
    virtual void processRequests(std::vector<std::shared_ptr<ReadRequest>> &batch) override;

private:
    /**
     * @brief Register all nodes that are configured to be registered.
     */
    void registerNodes();

    /**
     * @brief Rebuild nodeIds for all nodes that were registered.
     */
    void rebuildNodeIds();

    /**
     * @brief Rebuild the namespace index map from the server's array.
     */
//    void updateNamespaceMap(const UaStringArray &nsArray);

    static std::map<std::string, SessionOpen62541 *> sessions;    /**< session management */

    const std::string name;                                   /**< unique session name */
    const std::string serverURL;                              /**< server URL */
    bool autoConnect;                                         /**< auto (re)connect flag */
    std::map<std::string, SubscriptionOpen62541*> subscriptions;  /**< subscriptions on this session */
    std::vector<ItemOpen62541 *> items;                           /**< items on this session */
    UA_UInt32 registeredItemsNo;                           /**< number of registered items */
    std::map<std::string, UA_UInt16> namespaceMap;         /**< local namespace map (URI->index) */
    std::map<UA_UInt16, UA_UInt16> nsIndexMap;          /**< namespace index map (local->server-side) */
//    UaSession* puasession;                                    /**< pointer to low level session */
//    SessionConnectInfo connectInfo;                           /**< connection metadata */
//    SessionSecurityInfo securityInfo;                         /**< security metadata */
//    UaClient::ServerStatus serverConnectionStatus;            /**< connection status for this session */

    int transactionId;                                        /**< next transaction id */
    /** itemOpen62541 vectors of outstanding read or write operations, indexed by transaction id */
    std::map<UA_UInt32, std::unique_ptr<std::vector<ItemOpen62541 *>>> outstandingOps;
    epicsMutex opslock;                                       /**< lock for outstandingOps map */

    RequestQueueBatcher<WriteRequest> writer;                 /**< batcher for write requests */
    unsigned int writeNodesMax;                               /**< max number of nodes per write request */
    unsigned int writeTimeoutMin;                             /**< timeout after write request batch of 1 node [ms] */
    unsigned int writeTimeoutMax;                             /**< timeout after write request of NodesMax nodes [ms] */
    RequestQueueBatcher<ReadRequest> reader;                  /**< batcher for read requests */
    unsigned int readNodesMax;                                /**< max number of nodes per read request */
    unsigned int readTimeoutMin;                              /**< timeout after read request batch of 1 node [ms] */
    unsigned int readTimeoutMax;                              /**< timeout after read request batch of NodesMax nodes [ms] */

    /** open62541 interfaces */
    static UA_ClientConfig defaultClientConfig;               /**< configuration for each new session */
    UA_Client *client;                                        /**< low level handle for this session */
    epicsMutex clientlock;                                    /**< lock for client implementation */
    UA_StatusCode connectStatus;
    UA_SecureChannelState channelState;
    UA_SessionState sessionState;


    /**
     * @brief EPICS Timer callback to poll client session
     */
    epicsTimerNotify::expireStatus expire (const epicsTime & currentTime );

    epicsTimerQueueActive &timerQueue;                        /**< for polling the client */
    epicsTimer &sessionPollTimer;                             /**< for polling the client */

    /**
     * @brief Initialize global resources.
     */
    static void initOnce(void*);

    /**
     * @brief Client connection status callback.
     */
    void connectionStatusChanged(
            UA_SecureChannelState channelState,
            UA_SessionState sessionState,
            UA_StatusCode connectStatus);
};

} // namespace DevOpcua

#endif // DEVOPCUA_SESSIONOPEN62541_H
