/*************************************************************************\
* Copyright (c) 2018-2019 ITER Organization.
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
#include <memory>

#include <uabase.h>
#include <uaclientsdk.h>
#include <uasession.h>

#include <epicsMutex.h>
#include <epicsTypes.h>
#include <initHooks.h>

#include "RequestQueueBatcher.h"
#include "Session.h"

namespace DevOpcua {

using namespace UaClientSdk;

class SubscriptionUaSdk;
class ItemUaSdk;
struct WriteRequest;
struct ReadRequest;

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
     * @param autoConnect        if true (default), client will automatically connect
     *                           both initially and after connection loss
     * @param debug              initial debug verbosity level
     * @param batchNodes         max. number of node to use in any single service call
     * @param clientCertificate  path to client-side certificate
     * @param clientPrivateKey   path to client-side private key
     */
    SessionUaSdk(const std::string &name, const std::string &serverUrl,
                 bool autoConnect = true, int debug = 0, epicsUInt32 batchNodes = 0,
                 const char *clientCertificate = nullptr, const char *clientPrivateKey = nullptr);
    ~SessionUaSdk() override;

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
    UaStructureDefinition structureDefinition(const UaNodeId &dataTypeId)
    { return puasession->structureDefinition(dataTypeId); }

    /**
     * @brief Request a beginRead service for an item
     *
     * @param item  item to request beginRead for
     */
    void requestRead(ItemUaSdk &item, const unsigned short priority);

    /**
     * @brief Request a beginWrite service for an item
     *
     * @param item  item to request beginWrite for
     */
    void requestWrite(ItemUaSdk &item, const unsigned short priority);

    /**
     * @brief Initiate read of all nodes.
     */
    void readAllNodes();

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

    /**
     * @brief Set an option for the session. See DevOpcua::Session::setOption
     */
    virtual void setOption(const std::string &name, const std::string &value) override;

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

    // Get a new (unique) transaction id
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
     * @brief Issue read request for one batch of nodes.
     */
    void readBatchOfNodes (std::vector<ItemUaSdk *>::iterator from,
                           std::vector<ItemUaSdk *>::iterator to);

    static std::map<std::string, SessionUaSdk *> sessions;    /**< session management */

    const std::string name;                                   /**< unique session name */
    UaString serverURL;                                       /**< server URL */
    bool autoConnect;                                         /**< auto (re)connect flag */
    std::map<std::string, SubscriptionUaSdk*> subscriptions;  /**< subscriptions on this session */
    std::vector<ItemUaSdk *> items;                           /**< items on this session */
    OpcUa_UInt32 registeredItemsNo;                           /**< number of registered items */
    UaSession* puasession;                                    /**< pointer to low level session */
    SessionConnectInfo connectInfo;                           /**< connection metadata */
    SessionSecurityInfo securityInfo;                         /**< security metadata */
    UaClient::ServerStatus serverConnectionStatus;            /**< connection status for this session */
    int transactionId;                                        /**< next transaction id */
    /** itemUaSdk vectors of outstanding read or write operations, indexed by transaction id */
    std::map<OpcUa_UInt32, std::unique_ptr<std::vector<ItemUaSdk *>>> outstandingOps;
    epicsMutex opslock;                                       /**< lock for outstandingOps map */
    RequestQueueBatcher<WriteRequest> writer;                 /**< batcher for write requests */
    RequestQueueBatcher<ReadRequest> reader;                  /**< batcher for read requests */
};

} // namespace DevOpcua

#endif // DEVOPCUA_SESSIONUASDK_H
