/*************************************************************************\
* Copyright (c) 2018-2021 ITER Organization.
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

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <uabase.h>
#include <uaclientsdk.h>
#include <uasession.h>
#include <uaenumdefinition.h>

#include <epicsString.h>
#include <epicsMutex.h>
#include <epicsTypes.h>
#include <initHooks.h>

#include "RequestQueueBatcher.h"
#include "Session.h"
#include "Registry.h"
#include "DataElement.h"

namespace DevOpcua {

using namespace UaClientSdk;

class SubscriptionUaSdk;
class ItemUaSdk;
struct WriteRequest;
struct ReadRequest;

/**
 * @brief Enum for the requested security mode
 */
enum RequestedSecurityMode { Best, None, Sign, SignAndEncrypt };

/**
 * @brief The SessionUaSdk implementation of an OPC UA client session.
 *
 * See DevOpcua::Session
 *
 * The connect call establishes and maintains a Session with a Server.
 * After a successful connect, the connection is monitored by the low level driver.
 * Connection status changes are reported through the callback
 * UaClientSdk::UaSessionCallback::connectionStatusChanged.
 *
 * The disconnect call disconnects the Session, deleting all Subscriptions
 * and freeing all related resources on both server and client.
 */

class SessionUaSdk
        : public UaSessionCallback
        , public Session
        , public RequestConsumer<WriteRequest>
        , public RequestConsumer<ReadRequest>
{
    UA_DISABLE_COPY(SessionUaSdk);
    friend class SubscriptionUaSdk;

public:
    /**
     * @brief Create an OPC UA session.
     *
     * @param name               session name (used in EPICS record configuration)
     * @param serverUrl          OPC UA server URL
     */
    SessionUaSdk(const std::string &name, const std::string &serverUrl);
    ~SessionUaSdk() override;

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
     * @brief Get pointer to enumChoices if argument refers to enum type, else nullptr
     */
    const EnumChoices* getEnumChoices(const UaEnumDefinition& enumDefinition) const;

    const EnumChoices* getEnumChoices(const UaNodeId* typeId) const
    { return typeId ? getEnumChoices(puasession->enumDefinition(*typeId)) : nullptr; }

    /**
     * @brief Get a structure definition from the session dictionary.
     * @param dataTypeId data type of the extension object
     * @return structure definition
     */
    UaStructureDefinition structureDefinition(const UaNodeId &dataTypeId)
    { return puasession->structureDefinition(dataTypeId); }

    /**
     * @brief Request a beginRead service for an item
     *
     * @param item  item to request beginRead for
     */
    void requestRead(ItemUaSdk &item);

    /**
     * @brief Request a beginWrite service for an item
     *
     * @param item  item to request beginWrite for
     */
    void requestWrite(ItemUaSdk &item);

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
    static SessionUaSdk *
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
    virtual void addNamespaceMapping(const OpcUa_UInt16 nsIndex, const std::string &uri) override;

    unsigned int noOfSubscriptions() const { return static_cast<unsigned int>(subscriptions.size()); }
    unsigned int noOfItems() const { return static_cast<unsigned int>(items.size()); }

    /**
     * @brief Add an item to the session.
     *
     * @param item  item to add
     */
    void addItemUaSdk(ItemUaSdk *item);

    /**
     * @brief Remove an item from the session.
     *
     * @param item  item to remove
     */
    void removeItemUaSdk(ItemUaSdk *item);

    /**
     * @brief Map namespace index (local -> server)
     *
     * @param nsIndex  local namespace index
     *
     * @return server-side namespace index
     */
    OpcUa_UInt16 mapNamespaceIndex(const OpcUa_UInt16 nsIndex) const;

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
    OpcUa_UInt32 getTransactionId();

    // UaSessionCallback interface
    virtual void connectionStatusChanged(
            OpcUa_UInt32 clientConnectionId,
            UaClient::ServerStatus serverStatus) override;

    virtual void readComplete(
            OpcUa_UInt32 transactionId,
            const UaStatus &result,
            const UaDataValues &values,
            const UaDiagnosticInfos &diagnosticInfos) override;

    virtual void writeComplete(
            OpcUa_UInt32 transactionId,
            const UaStatus &result,
            const UaStatusCodeArray &results,
            const UaDiagnosticInfos &diagnosticInfos) override;

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
    void updateNamespaceMap(const UaStringArray &nsArray);

    enum ConnectResult { fatal = -1, ok = 0
                         , cantConnect
                         , noMatchingEndpoint
                       };
    const char *connectResultString(const ConnectResult result);

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

    static Registry<SessionUaSdk> sessions;                   /**< session management */

    UaString serverURL;                                       /**< server URL */
    std::map<std::string, SubscriptionUaSdk*> subscriptions;  /**< subscriptions on this session */
    std::vector<ItemUaSdk *> items;                           /**< items on this session */
    OpcUa_UInt32 registeredItemsNo;                           /**< number of registered items */
    std::map<std::string, OpcUa_UInt16> namespaceMap;         /**< local namespace map (URI->index) */
    std::map<OpcUa_UInt16, OpcUa_UInt16> nsIndexMap;          /**< namespace index map (local->server-side) */
    UaSession* puasession;                                    /**< pointer to low level session */
    SessionConnectInfo connectInfo;                           /**< connection metadata */
    SessionSecurityInfo securityInfo;                         /**< security metadata */
    unsigned char securityLevel;                              /**< actual security level */
    RequestedSecurityMode reqSecurityMode;                    /**< requested security mode */
    UaString reqSecurityPolicyURI;                            /**< requested security policy */
    UaClient::ServerStatus serverConnectionStatus;            /**< connection status for this session */
    int transactionId;                                        /**< next transaction id */
    /** itemUaSdk vectors of outstanding read or write operations, indexed by transaction id */
    std::map<OpcUa_UInt32, std::unique_ptr<std::vector<ItemUaSdk *>>> outstandingOps;
    epicsMutex opslock;                                       /**< lock for outstandingOps map */

    RequestQueueBatcher<WriteRequest> writer;                 /**< batcher for write requests */
    unsigned int writeNodesMax;                               /**< max number of nodes per write request */
    unsigned int writeTimeoutMin;                             /**< timeout after write request batch of 1 node [ms] */
    unsigned int writeTimeoutMax;                             /**< timeout after write request of NodesMax nodes [ms] */
    RequestQueueBatcher<ReadRequest> reader;                  /**< batcher for read requests */
    unsigned int readNodesMax;                                /**< max number of nodes per read request */
    unsigned int readTimeoutMin;                              /**< timeout after read request batch of 1 node [ms] */
    unsigned int readTimeoutMax;                              /**< timeout after read request batch of NodesMax nodes [ms] */
};

} // namespace DevOpcua

#endif // DEVOPCUA_SESSIONUASDK_H
