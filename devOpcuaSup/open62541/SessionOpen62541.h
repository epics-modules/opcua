/*************************************************************************\
* Copyright (c) 2018-2021 ITER Organization.
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
#include <set>

#include <open62541/client_config_default.h>

#include <epicsString.h>
#include <epicsMutex.h>
#include <epicsTypes.h>
#include <epicsThread.h>
#include <initHooks.h>

#include "RequestQueueBatcher.h"
#include "Session.h"
#include "Registry.h"

namespace DevOpcua {

class SubscriptionOpen62541;
class ItemOpen62541;
struct WriteRequest;
struct ReadRequest;

/**
 * @brief Enum for the requested security mode
 */
enum RequestedSecurityMode { Best, None, Sign, SignAndEncrypt };

// print some UA types

inline std::ostream& operator << (std::ostream& os, const UA_String& ua_string)
{
    // ua_string.data is not terminated!
    return os.write(reinterpret_cast<const char*>(ua_string.data), ua_string.length);
}

inline std::ostream& operator << (std::ostream& os, const UA_LocalizedText& ua_localizedText)
{
    if (ua_localizedText.locale.length) os << '[' << ua_localizedText.locale << ']';
    return os << '"' << ua_localizedText.text << '"';
}

std::ostream& operator << (std::ostream& os, const UA_NodeId& ua_nodeId);

std::ostream& operator << (std::ostream& os, const UA_Variant &ua_variant);

// Open62541 has no ClientSecurityInfo structure
// Make our own for convenience
struct ClientSecurityInfo {
    UA_ByteString serverCertificate;
    UA_ByteString clientCertificate;
    UA_ByteString privateKey;
    UA_ExtensionObject userIdentityToken;
    UA_String securityPolicyUri;
    UA_MessageSecurityMode securityMode;
    ClientSecurityInfo () {
        serverCertificate = UA_STRING_NULL;
        clientCertificate = UA_STRING_NULL;
        privateKey = UA_STRING_NULL;
        userIdentityToken = {};
        securityPolicyUri = UA_STRING_NULL;
        securityMode = UA_MESSAGESECURITYMODE_NONE;
    }
    ~ClientSecurityInfo () {
        UA_ExtensionObject_clear(&userIdentityToken);
        UA_ByteString_clear(&privateKey);
        UA_ByteString_clear(&clientCertificate);
        UA_ByteString_clear(&serverCertificate);
        UA_String_clear(&securityPolicyUri);
    }
};

/**
 * @brief The SessionOpen62541 implementation of an open62541 client session.
 *
 * See DevOpcua::Session
 *
 * The connect call establishes and maintains a Session with a Server.
 * After a successful connect, the connection is monitored by the low level driver.
 * Connection status changes are reported through the callback
 * SessionOpen62541::connectionStatusChanged.
 *
 * The disconnect call disconnects the Session, deleting all Subscriptions
 * and freeing all related resources on both server and client.
 */

class SessionOpen62541
        : public Session
        , public RequestConsumer<WriteRequest>
        , public RequestConsumer<ReadRequest>
        , public epicsThreadRunable
{
    // Cannot copy a Session
    SessionOpen62541(const SessionOpen62541 &) = delete;
    SessionOpen62541 &operator=(const SessionOpen62541 &) = delete;

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
                 bool autoConnect = true, int debug = 0);
    ~SessionOpen62541() override;

    /**
     * @brief Connect session. See DevOpcua::Session::connect
     * @return long status (0 = OK)
     */
    virtual long connect(bool manual=true) override;

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

    /**
     * @brief Do a discovery and show the available endpoints.
     */
    virtual void showSecurity() override;

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
//    UA_StructureDefinition structureDefinition(const UaNodeId &dataTypeId)
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

    /**
     * @brief Create all subscriptions related to this session.
     */
    void createAllSubscriptions();

    /**
     * @brief Add all monitored items to subscriptions related to this session.
     */
    void addAllMonitoredItems();

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
     * @param name  session name to search for
     *
     * @return  pointer to session, nullptr if not found
     */
    static SessionOpen62541 *
    find(const std::string &name)
    {
        return sessions.find(name);
    }

    static std::set<Session *>
    glob(const std::string &pattern)
    {
        return sessions.glob<Session>(pattern);
    }

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

    /**
     * @brief EPICS IOC Database initHook function.
     *
     * Hook function called when the EPICS IOC is being initialized.
     * Connects all sessions with autoConnect=true.
     *
     * @param state  initialization state
     */
    static void initHook(initHookState state);

    // Get a new (unique per session) transaction id
    UA_UInt32 getTransactionId();

    // Open62541 session callbacks
    void connectionStatusChanged(
            UA_SecureChannelState channelState,
            UA_SessionState sessionState,
            UA_StatusCode connectStatus);

    void readComplete(
            UA_UInt32 transactionId,
            UA_ReadResponse* response);

    void  writeComplete(
            UA_UInt32 transactionId,
            UA_WriteResponse* response);

    // RequestConsumer<> interfaces
    virtual void processRequests(std::vector<std::shared_ptr<WriteRequest>> &batch) override;
    virtual void processRequests(std::vector<std::shared_ptr<ReadRequest>> &batch) override;

    /**
     * @brief Setup ClientSecurityInfo object from PKI store locations and cert files
     */
    static void setupClientSecurityInfo(ClientSecurityInfo &securityInfo,
                                        const std::string *sessionName = nullptr,
                                        const int debug = 0);

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
    void updateNamespaceMap(const UA_String *nsArray, UA_UInt16 nsCount);

    enum ConnectResult { fatal = -1, ok = 0
                         , cantConnect
                         , noMatchingEndpoint
                       };
    const char* connectResultString (const ConnectResult result);

    /**
     * @brief Mark connection loss: clear request queues and process records.
     */
    void markConnectionLoss();

    /**
     * @brief Read user/pass or cert/key/pass credentials from credentials file.
     */
    void setupIdentity();

    /**
     * @brief Set up security.
     *
     * Discovers the endpoints and finds the one matching the user configuration.
     * Verifies the presented server certificate.
     *
     * @returns status (0 = OK)
     */
    ConnectResult setupSecurity();

    /**
     * @brief Asynchronous worker thread body.
     */
    virtual void run() override;

    // Wrapper for Session::securityPolicyString to match argument type
    static std::string securityPolicyString(const UA_String& policy)
    {
        return Session::securityPolicyString(
            std::string(reinterpret_cast<const char*>(policy.data), policy.length));
    }

    static Registry<SessionOpen62541> sessions;                   /**< session management */

    const std::string serverURL;                                  /**< server URL */
    std::map<std::string, SubscriptionOpen62541*> subscriptions;  /**< subscriptions on this session */
    std::vector<ItemOpen62541 *> items;                           /**< items on this session */
    UA_UInt32 registeredItemsNo;                                  /**< number of registered items */
    std::map<std::string, UA_UInt16> namespaceMap;                /**< local namespace map (URI->index) */
    std::map<UA_UInt16, UA_UInt16> nsIndexMap;                    /**< namespace index map (local->server-side) */

    ClientSecurityInfo securityInfo;                              /**< security metadata */
    unsigned int securityLevel;                                   /**< actual security level */
    RequestedSecurityMode reqSecurityMode;                        /**< requested security mode */
    std::string reqSecurityPolicyUri;                             /**< requested security policy */

    int transactionId;                                            /**< next transaction id */
    /** itemOpen62541 vectors of outstanding read or write operations, indexed by transaction id */
    std::map<UA_UInt32, std::unique_ptr<std::vector<ItemOpen62541 *>>> outstandingOps;
    epicsMutex opslock;                                           /**< lock for outstandingOps map */

    RequestQueueBatcher<WriteRequest> writer;                     /**< batcher for write requests */
    unsigned int writeNodesMax;                                   /**< max number of nodes per write request */
    unsigned int writeTimeoutMin;                                 /**< timeout after write request batch of 1 node [ms] */
    unsigned int writeTimeoutMax;                                 /**< timeout after write request of NodesMax nodes [ms] */
    RequestQueueBatcher<ReadRequest> reader;                      /**< batcher for read requests */
    unsigned int readNodesMax;                                    /**< max number of nodes per read request */
    unsigned int readTimeoutMin;                                  /**< timeout after read request batch of 1 node [ms] */
    unsigned int readTimeoutMax;                                  /**< timeout after read request batch of NodesMax nodes [ms] */

    /** open62541 interfaces */
    UA_Client *client;                                            /**< low level handle for this session */
    epicsMutex clientlock;                                        /**< lock for client implementation */
    UA_SecureChannelState channelState;                           /**< status for this session */
    UA_SessionState sessionState;                                 /**< status for this session */
    UA_StatusCode connectStatus;                                  /**< status for this session */
    unsigned int MaxNodesPerRead;                                 /**< server max number of nodes per write request */
    unsigned int MaxNodesPerWrite;                                /**< server max number of nodes per write request */
    epicsThread *workerThread;                                    /**< Asynchronous worker thread */
};

} // namespace DevOpcua

#endif // DEVOPCUA_SESSIONOPEN62541_H
