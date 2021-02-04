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

// Avoid problems on Windows (macros min, max clash with numeric_limits<>)
#ifdef _WIN32
#  define NOMINMAX
#endif

#include <iostream>
#include <string>
#include <map>
#include <algorithm>
#include <utility>
#include <vector>
#include <limits>

#include <epicsExit.h>
#include <epicsThread.h>
#include <epicsAtomic.h>
#include <initHooks.h>
#include <errlog.h>

#include <open62541/client.h>

#define epicsExportSharedSymbols
#include "Session.h"
#include "RecordConnector.h"
#include "RequestQueueBatcher.h"
#include "SessionOpen62541.h"
#include "SubscriptionOpen62541.h"
#include "DataElementOpen62541.h"
#include "ItemOpen62541.h"

namespace DevOpcua {

double PollPeriod = 0.01;

std::map<std::string, SessionOpen62541*> SessionOpen62541::sessions;

// Cargo structure and batcher for write requests
struct WriteRequest {
    ItemOpen62541 *item;
//    OpcUa_WriteValue wvalue;
};

// Cargo structure and batcher for read requests
struct ReadRequest {
    ItemOpen62541 *item;
};

inline const char *
toStr(UA_SecureChannelState channelState) {
    switch (channelState) {
        case UA_SECURECHANNELSTATE_CLOSED:       return "Closed";
        case UA_SECURECHANNELSTATE_HEL_SENT:     return "HELSent";
        case UA_SECURECHANNELSTATE_HEL_RECEIVED: return "HELReceived";
        case UA_SECURECHANNELSTATE_ACK_SENT:     return "ACKSent";
        case UA_SECURECHANNELSTATE_ACK_RECEIVED: return "AckReceived";
        case UA_SECURECHANNELSTATE_OPN_SENT:     return "OPNSent";
        case UA_SECURECHANNELSTATE_OPEN:         return "Open";
        case UA_SECURECHANNELSTATE_CLOSING:      return "Closing";
        default:                                 return "<unknown>";
    }
}

inline const char *
toStr(UA_SessionState sessionState) {
    switch (sessionState) {
        case UA_SESSIONSTATE_CLOSED:             return "Closed";
        case UA_SESSIONSTATE_CREATE_REQUESTED:   return "CreateRequested";
        case UA_SESSIONSTATE_CREATED:            return "Created";
        case UA_SESSIONSTATE_ACTIVATE_REQUESTED: return "ActivateRequested";
        case UA_SESSIONSTATE_ACTIVATED:          return "Activated";
        case UA_SESSIONSTATE_CLOSING:            return "Closing";
        default:                                 return "<unknown>";
    }
}
/*
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
*/

UA_ClientConfig SessionOpen62541::defaultClientConfig;

void SessionOpen62541::initOnce(void*)
{
    errlogPrintf("SessionOpen62541::initOnce\n");
    if (UA_STATUSCODE_GOOD != UA_ClientConfig_setDefault(&defaultClientConfig))
    {
        /* unrecoverable failure */
    }

    /* set clientDescription */
    UA_ApplicationDescription_clear(&defaultClientConfig.clientDescription);
    defaultClientConfig.clientDescription.applicationType = UA_APPLICATIONTYPE_CLIENT;
    defaultClientConfig.clientDescription.applicationName = UA_LOCALIZEDTEXT_ALLOC("en-US", "EPICS IOC");
    defaultClientConfig.clientDescription.productUri = UA_STRING_ALLOC("urn:EPICS:IOC");
    char applicationUri[HOST_NAME_MAX+15] = { "urn:" };
    if (gethostname(applicationUri+4, HOST_NAME_MAX) != 0)
        strcpy(applicationUri+4, "unknown-host");
    strcat(applicationUri, ":EPICS:IOC");
    defaultClientConfig.clientDescription.applicationUri = UA_STRING_ALLOC(applicationUri);

    /* set callbacks */
    defaultClientConfig.stateCallback = [] (UA_Client *client,
        UA_SecureChannelState channelState, UA_SessionState sessionState, UA_StatusCode connectStatus) {
            static_cast<SessionOpen62541*>(UA_Client_getContext(client))->
                connectionStatusChanged(channelState, sessionState, connectStatus);
        };
    defaultClientConfig.subscriptionInactivityCallback = [] (UA_Client *client,
        UA_UInt32 subscriptionId, void *subContext) {
            static_cast<SubscriptionOpen62541*>(subContext)->subscriptionInactive(subscriptionId);
        };
    defaultClientConfig.connectivityCheckInterval = 1000; // [ms]
    defaultClientConfig.outStandingPublishRequests = 5;

/*
    connectInfo.bAutomaticReconnect = autoConnect;
    connectInfo.bRetryInitialConnect = autoConnect;
    connectInfo.nMaxOperationsPerServiceCall = batchNodes;

    connectInfo.typeDictionaryMode = UaClientSdk::UaClient::ReadTypeDictionaries_Reconnect;
*/

    initHookRegister(SessionOpen62541::initHook);
    epicsAtExit(SessionOpen62541::atExit, nullptr);
    errlogPrintf("SessionOpen62541::initOnce DONE\n");
}

SessionOpen62541::SessionOpen62541 (const std::string &name, const std::string &serverUrl,
                            bool autoConnect, int debug, epicsUInt32 batchNodes,
                            const char *clientCertificate, const char *clientPrivateKey)
    : Session(debug)
    , name(name)
    , serverURL(serverUrl)
    , autoConnect(autoConnect)
    , registeredItemsNo(0)
    , transactionId(0)
    , writer("OPCwr-" + name, *this, batchNodes)
    , writeNodesMax(0)
    , writeTimeoutMin(0)
    , writeTimeoutMax(0)
    , reader("OPCrd-" + name, *this, batchNodes)
    , readNodesMax(0)
    , readTimeoutMin(0)
    , readTimeoutMax(0)
    , connectStatus(UA_STATUSCODE_BADINVALIDSTATE)
    , channelState(UA_SECURECHANNELSTATE_CLOSED)
    , sessionState(UA_SESSIONSTATE_CLOSED)
    , timerQueue(epicsTimerQueueActive::allocate (false,epicsThreadPriorityLow))
    , sessionPollTimer(timerQueue.createTimer())
{
    errlogPrintf("SessionOpen62541 constructor %s\n", name.c_str());
    static epicsThreadOnceId init_once_id = EPICS_THREAD_ONCE_INIT;
    epicsThreadOnce(&init_once_id, initOnce, nullptr);

    client = UA_Client_newWithConfig(&defaultClientConfig);
    UA_ClientConfig *config = UA_Client_getConfig(client);
    config->clientContext = this;

/*
    connectInfo.sSessionName     = UaString(name.c_str());

    connectInfo.bAutomaticReconnect = autoConnect;
    connectInfo.bRetryInitialConnect = autoConnect;
    connectInfo.nMaxOperationsPerServiceCall = batchNodes;

    connectInfo.typeDictionaryMode = UaClientSdk::UaClient::ReadTypeDictionaries_Reconnect;
*/

    //TODO: init security settings
    if ((clientCertificate && (clientCertificate[0] != '\0'))
            || (clientPrivateKey && (clientPrivateKey[0] != '\0')))
        errlogPrintf("OPC UA security not supported yet\n");

    sessions[name] = this;

    errlogPrintf("SessionOpen62541 constructor %s DONE\n", name.c_str());
}

const std::string &
SessionOpen62541::getName () const
{
    return name;
}

UA_UInt32
SessionOpen62541::getTransactionId ()
{
    return static_cast<UA_UInt32>(epics::atomic::increment(transactionId));
}

SessionOpen62541 &
SessionOpen62541::findSession (const std::string &name)
{
    auto it = sessions.find(name);
    if (it == sessions.end()) {
        throw std::runtime_error("no such session");
    }
    return *(it->second);
}

bool
SessionOpen62541::sessionExists (const std::string &name)
{
    auto it = sessions.find(name);
    return !(it == sessions.end());
}

void
SessionOpen62541::setOption (const std::string &name, const std::string &value)
{
    bool updateReadBatcher = false;
    bool updateWriteBatcher = false;

    if (name == "clientcert") {
        errlogPrintf("security not implemented\n");
    } else if (name == "clientkey") {
        errlogPrintf("security not implemented\n");
    } else if (name == "batch-nodes") {
        errlogPrintf("DEPRECATED: option 'batch-nodes'; use 'nodes-max' instead\n");
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
//        connectInfo.nMaxOperationsPerServiceCall = ul;
        updateReadBatcher = true;
        updateWriteBatcher = true;
    } else if (name == "nodes-max") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
//        connectInfo.nMaxOperationsPerServiceCall = ul;
        updateReadBatcher = true;
        updateWriteBatcher = true;
    } else if (name == "read-nodes-max") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        readNodesMax = ul;
        updateReadBatcher = true;
    } else if (name == "read-timeout-min") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        readTimeoutMin = ul;
        updateReadBatcher = true;
    } else if (name == "read-timeout-max") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        readTimeoutMax = ul;
        updateReadBatcher = true;
    } else if (name == "write-nodes-max") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        writeNodesMax = ul;
        updateWriteBatcher = true;
    } else if (name == "write-timeout-min") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        writeTimeoutMin = ul;
        updateWriteBatcher = true;
    } else if (name == "write-timeout-max") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        writeTimeoutMax = ul;
        updateWriteBatcher = true;
    } else {
        errlogPrintf("unknown option '%s' ignored\n", name.c_str());
    }

    unsigned int max = 0;
/*
    if (connectInfo.nMaxOperationsPerServiceCall > 0 && readNodesMax > 0) {
        max = std::min(connectInfo.nMaxOperationsPerServiceCall, readNodesMax);
    } else {
        max = connectInfo.nMaxOperationsPerServiceCall + readNodesMax;
    }
*/
    if (updateReadBatcher) reader.setParams(max, readTimeoutMin, readTimeoutMax);

/*
    if (connectInfo.nMaxOperationsPerServiceCall > 0 && writeNodesMax > 0) {
        max = std::min(connectInfo.nMaxOperationsPerServiceCall, writeNodesMax);
    } else {
        max = connectInfo.nMaxOperationsPerServiceCall + writeNodesMax;
    }
*/
    if (updateWriteBatcher) writer.setParams(max, writeTimeoutMin, writeTimeoutMax);
}

epicsTimerNotify::expireStatus SessionOpen62541::expire (const epicsTime &currentTime)
{
    // Currently (open62541 version 1.2), the client has no internal mechanism
    // to run asynchronous tasks. We need to call UA_Client_run_iterate() regularly
    // for asynchronous events to happen.
    // Also until now, the client is not thread save. We have to protect it
    // with our own mutex.
    if (client)
    {
        Guard G(clientlock);
        if (UA_Client_run_iterate(client, 0) == UA_STATUSCODE_GOOD)
        return expireStatus(restart, PollPeriod);
        // One last time read status
        UA_Client_getState(client, &channelState, &sessionState, &connectStatus);
    }
    return noRestart;
}

long
SessionOpen62541::connect ()
{
    errlogPrintf("SessionOpen62541::connect %s\n", name.c_str());
    if (!client) {
        std::cerr << "Session " << name.c_str()
                  << ": invalid session, cannot connect" << std::endl;
        return -1;
    }
    if (isConnected()) {
        if (debug) std::cerr << "Session " << name.c_str() << ": already connected ("
                             << toStr(sessionState) << "," << toStr(channelState) << ")" << std::endl;
        return 0;
    }
    errlogPrintf("SessionOpen62541::connect %s: connecting to %s\n", name.c_str(), serverURL.c_str());
    {
        Guard G(clientlock);
        connectStatus = UA_Client_connectAsync(client, serverURL.c_str());
    }
    if (connectStatus != UA_STATUSCODE_GOOD) {
        std::cerr << "Session " << name.c_str()
                  << ": connect service failed with status "
                  << UA_StatusCode_name(connectStatus) << std::endl;
        return -1;
    }
    if (debug) std::cerr << "Session " << name.c_str()
                         << ": connect service ok" << std::endl;
    // asynchronous: remaining actions are done on the connectionStatusChanged callback
    sessionPollTimer.start(*this, 0);
    errlogPrintf("SessionOpen62541::connect %s DONE\n", name.c_str());
    return 0;
}

long
SessionOpen62541::disconnect ()
{
    errlogPrintf("SessionOpen62541::disconnect %s\n", name.c_str());
    sessionPollTimer.cancel();
    if (client && (channelState != UA_SECURECHANNELSTATE_CLOSED ||
                   sessionState != UA_SESSIONSTATE_CLOSED)) {
        Guard G(clientlock);
        // Close the session and the secure channel
        // and remove all subscriptions.
        // Cannot fail.
        UA_Client_disconnect(client);
    }
    // Detach all subscriptions of this session from driver
    for (auto &it : subscriptions) {
        it.second->clear();
    }
    errlogPrintf("SessionOpen62541::disconnect %s DONE\n", name.c_str());
    return 0;
}

bool
SessionOpen62541::isConnected () const
{
    if (!client) return false;
    if (connectStatus != UA_STATUSCODE_GOOD) return false;
    if (channelState != UA_SECURECHANNELSTATE_OPEN) return false;
    if (sessionState != UA_SESSIONSTATE_ACTIVATED) return false;
    return true;
}

void
SessionOpen62541::requestRead (ItemOpen62541 &item)
{
    auto cargo = std::make_shared<ReadRequest>();
    cargo->item = &item;
    reader.pushRequest(cargo, item.recConnector->getRecordPriority());
}

// Low level reader function called by the RequestQueueBatcher
void
SessionOpen62541::processRequests (std::vector<std::shared_ptr<ReadRequest>> &batch)
{
    errlogPrintf("SessionOpen62541::processRequests ReadRequest UNIMPLEMENTED\n");
/*
    UA_StatusCode status;
    UaReadValueIds nodesToRead;
    std::unique_ptr<std::vector<ItemOpen62541 *>> itemsToRead(new std::vector<ItemOpen62541 *>);
    ServiceSettings serviceSettings;
    UA_UInt32 id = getTransactionId();

    nodesToRead.create(batch.size());
    UA_UInt32 i = 0;
    for (auto c : batch) {
        c->item->getNodeId().copyTo(&nodesToRead[i].NodeId);
        nodesToRead[i].AttributeId = OpcUa_Attributes_Value;
        itemsToRead->push_back(c->item);
        i++;
    }

    if (isConnected()) {
        Guard G(opslock);
        status = puasession->beginRead(serviceSettings,                // Use default settings
                                       0,                              // Max age
                                       OpcUa_TimestampsToReturn_Both,  // Time stamps to return
                                       nodesToRead,                    // Array of nodes to read
                                       id);                            // Transaction id

	    if (!UA_STATUS_IS_BAD(status)) {
	        errlogPrintf("OPC UA session %s: (requestRead) beginRead service failed with status %s\n",
	                     name.c_str(), UA_StatusCode_name(status);
            //TODO: create writeFailure events for all items of the batch
//	        item.setIncomingEvent(ProcessReason::readFailure);

        } else {
            if (debug >= 5)
                std::cout << "Session " << name.c_str()
                          << ": (requestRead) beginRead service ok"
                          << " (transaction id " << id
                          << "; retrieving " << nodesToRead.length() << " nodes)" << std::endl;
            outstandingOps.insert(std::pair<UA_UInt32, std::unique_ptr<std::vector<ItemOpen62541 *>>>
                                  (id, std::move(itemsToRead)));
        }
    }
*/
}

void
SessionOpen62541::requestWrite (ItemOpen62541 &item)
{
    auto cargo = std::make_shared<WriteRequest>();
    cargo->item = &item;
//    item.getOutgoingData().copyTo(&cargo->wvalue.Value.Value);
    item.clearOutgoingData();
    writer.pushRequest(cargo, item.recConnector->getRecordPriority());
}

// Low level writer function called by the RequestQueueBatcher
void
SessionOpen62541::processRequests (std::vector<std::shared_ptr<WriteRequest>> &batch)
{
    errlogPrintf("SessionOpen62541::processRequests WriteRequest UNIMPLEMENTED\n");
/*
    UA_StatusCode status;
    UaWriteValues nodesToWrite;
    std::unique_ptr<std::vector<ItemOpen62541 *>> itemsToWrite(new std::vector<ItemOpen62541 *>);
    ServiceSettings serviceSettings;
    UA_UInt32 id = getTransactionId();

    nodesToWrite.create(batch.size());
    UA_UInt32 i = 0;
    for (auto c : batch) {
        c->item->getNodeId().copyTo(&nodesToWrite[i].NodeId);
        nodesToWrite[i].AttributeId = OpcUa_Attributes_Value;
        nodesToWrite[i].Value.Value = c->wvalue.Value.Value;
        itemsToWrite->push_back(c->item);
        i++;
    }

    if (isConnected()) {
        Guard G(opslock);
        status = puasession->beginWrite(serviceSettings,        // Use default settings
                                        nodesToWrite,           // Array of nodes/data to write
                                        id);                    // Transaction id

        if (UA_STATUS_IS_BAD(status)) {
            errlogPrintf("OPC UA session %s: (requestWrite) beginWrite service failed with status %s\n",
                         name.c_str(), UA_StatusCode_name(status));
            //TODO: create writeFailure events for all items of the batch
//          item.setIncomingEvent(ProcessReason::writeFailure);

        } else {
            if (debug >= 5)
                std::cout << "Session " << name.c_str()
                          << ": (requestWrite) beginWrite service ok"
                          << " (transaction id " << id
                          << "; writing " << nodesToWrite.length() << " nodes)" << std::endl;
            outstandingOps.insert(std::pair<UA_UInt32, std::unique_ptr<std::vector<ItemOpen62541 *>>>
                                  (id, std::move(itemsToWrite)));
        }
    }
*/
}

void
SessionOpen62541::createAllSubscriptions ()
{
    errlogPrintf("SessionOpen62541::createAllSubscriptions session %s [%zu]\n", name.c_str(), subscriptions.size());
    for (auto &it : subscriptions) {
        it.second->create();
    }
    errlogPrintf("SessionOpen62541::createAllSubscriptions DONE\n");
}

void
SessionOpen62541::addAllMonitoredItems ()
{
    errlogPrintf("SessionOpen62541::addAllMonitoredItems session %s %zu subscriptions\n", name.c_str(), subscriptions.size());
    for (auto &it : subscriptions) {
        it.second->addMonitoredItems();
    }
    errlogPrintf("SessionOpen62541::addAllMonitoredItems DONE\n");
}

void
SessionOpen62541::registerNodes ()
{
    errlogPrintf("SessionOpen62541::registerNodes\n");
    UA_RegisterNodesRequest request;
    UA_RegisterNodesRequest_init(&request);
    request.nodesToRegister = static_cast<UA_NodeId*>(UA_Array_new(items.size(), &UA_TYPES[UA_TYPES_NODEID]));
    UA_UInt32 i = 0;
    for (auto &it : items) {
        if (it->linkinfo.registerNode) {
            it->show(0);
            request.nodesToRegister[i] = it->getNodeId();
            i++;
        }
    }
    errlogPrintf("SessionOpen62541::registerNodes %u of %zu nodes to register\n", registeredItemsNo, items.size());
    registeredItemsNo = i;
    request.nodesToRegisterSize = registeredItemsNo;
    if (registeredItemsNo) {
        UA_RegisterNodesResponse response = UA_Client_Service_registerNodes(client, request);
        if (response.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
            errlogPrintf("OPC UA session %s: (registerNodes) registerNodes service failed with status %s\n",
                         name.c_str(), UA_StatusCode_name(response.responseHeader.serviceResult));
        } else {
            if (debug)
                std::cout << "Session " << name.c_str()
                          << ": (registerNodes) registerNodes service ok"
                          << " (" << response.registeredNodeIdsSize << " nodes registered)" << std::endl;
            i = 0;
            for (auto &it : items) {
                if (it->linkinfo.registerNode) {
                    it->setRegisteredNodeId(response.registeredNodeIds[i]);
                    i++;
                }
            }
            registeredItemsNo = i;
        }
    }
    UA_Array_delete(request.nodesToRegister, items.size(), &UA_TYPES[UA_TYPES_NODEID]);
    errlogPrintf("SessionOpen62541::registerNodes [%u] DONE\n", registeredItemsNo);
}

void
SessionOpen62541::rebuildNodeIds ()
{
    errlogPrintf("SessionOpen62541::rebuildNodeIds [%zu]\n", items.size());
    for (auto &it : items) {
        it->rebuildNodeId();
    }
}

/* Add a mapping to the session's map, replacing any existing mappings with the same
 * index or URI */
void
SessionOpen62541::addNamespaceMapping (const unsigned short nsIndex, const std::string &uri)
{
    errlogPrintf("SessionOpen62541::addNamespaceMappingindex %u = %s\n", nsIndex, uri.c_str());
    for (auto &it : namespaceMap) {
        if (it.second == nsIndex) {
            errlogPrintf("SessionOpen62541::addNamespaceMappingindex: erase old %u = %s\n",
                nsIndex, it.first.c_str());
            namespaceMap.erase(it.first);
            break;
        }
    }
    auto it = namespaceMap.find(uri);
    if (it != namespaceMap.end())
        namespaceMap.erase(uri);
    namespaceMap.insert({uri, nsIndex});
}

/* If a local namespaceMap exists, create a local->remote numerical index mapping
 * for every URI that is found both there and in the server's array */
/*
void
SessionOpen62541::updateNamespaceMap(const UaStringArray &nsArray)
{
    if (debug)
        std::cout << "Session " << name.c_str()
                  << ": (updateNamespaceMap) namespace array with " << nsArray.length()
                  << " elements read; updating index map with " << namespaceMap.size()
                  << " entries" << std::endl;
    if (namespaceMap.size()) {
        nsIndexMap.clear();
        for (UA_UInt16 i = 0; i < nsArray.length(); i++) {
            auto it = namespaceMap.find(UaString(nsArray[i]).toUtf8());
            if (it != namespaceMap.end())
                nsIndexMap.insert({it->second, i});
        }
        // Report all local mappings that were not found on server
        for (auto it : namespaceMap) {
            if (nsIndexMap.find(it.second) == nsIndexMap.end()) {
                errlogPrintf("OPC UA session %s: locally mapped namespace '%s' not found on server\n",
                             name.c_str(), it.first.c_str());
            }
        }
    }
}
*/

void
SessionOpen62541::show (const int level) const
{
    std::cout << "session="      << name
              << " url="         << serverURL.c_str()
              << " connect status=" << UA_StatusCode_name(connectStatus)
              << " sessionState="   << toStr(sessionState)
              << " channelState="   << toStr(channelState)
              << " cert="        << "[none]"
              << " key="         << "[none]"
              << " debug="       << debug
              << " batch=";
/*
    if (isConnected())
        std::cout << puasession->maxOperationsPerServiceCall();
    else
*/
    std::cout << "?";
    std::cout // << "(" << connectInfo.nMaxOperationsPerServiceCall << ")"
//               << " autoconnect=" << (connectInfo.bAutomaticReconnect ? "y" : "n")
              << " items=" << items.size()
              << " registered=" << registeredItemsNo
              << " subscriptions=" << subscriptions.size()
              << " reader=" << reader.maxRequests() << "/"
              << reader.minHoldOff() << "-" << reader.maxHoldOff() << "ms"
              << " writer=" << writer.maxRequests() << "/"
              << writer.minHoldOff() << "-" << writer.maxHoldOff() << "ms"
              << std::endl;

    if (level >= 3) {
/*
        if (namespaceMap.size()) {
            std::cout << "Configured Namespace Mapping "
                      << "(local -> Namespace URI -> server)" << std::endl;
            for (auto p : namespaceMap) {
                std::cout << " " << p.second << " -> " << p.first << " -> "
                          << mapNamespaceIndex(p.second) << std::endl;
            }
        }
*/
    }

    if (level >= 1) {
        for (auto &it : subscriptions) {
            it.second->show(level-1);
        }
    }

    if (level >= 2) {
        if (items.size() > 0) {
            std::cerr << "subscription=[none]" << std::endl;
            for (auto &it : items) {
                if (!it->isMonitored()) it->show(level-1);
            }
        }
    }
}

void
SessionOpen62541::addItemOpen62541 (ItemOpen62541 *item)
{
    items.push_back(item);
}

void
SessionOpen62541::removeItemOpen62541 (ItemOpen62541 *item)
{
    auto it = std::find(items.begin(), items.end(), item);
    if (it != items.end())
        items.erase(it);
}

UA_UInt16
SessionOpen62541::mapNamespaceIndex (const UA_UInt16 nsIndex) const
{
    UA_UInt16 serverIndex = nsIndex;
    if (nsIndexMap.size()) {
        auto it = nsIndexMap.find(nsIndex);
        if (it != nsIndexMap.end())
            serverIndex = it->second;
    }
    return serverIndex;
}

// callbacks

void SessionOpen62541::connectionStatusChanged(
    UA_SecureChannelState newChannelState,
    UA_SessionState newSessionState,
    UA_StatusCode newConnectStatus)
{
    if (newConnectStatus != UA_STATUSCODE_GOOD) {
        connectStatus = newConnectStatus;
        errlogPrintf("open62541 session %s: Connection irrecoverably failed: %s\n",
                 name.c_str(), UA_StatusCode_name(connectStatus));
        return;
    }

    if (newChannelState != channelState) {
        errlogPrintf("open62541session %s: secure channel state changed from %s to %s\n",
            name.c_str(), toStr(channelState), toStr(newChannelState));
        channelState = newChannelState;
// TODO: What to do for each channelState change?
        switch (channelState) {
            case UA_SECURECHANNELSTATE_CLOSED:       break;
            case UA_SECURECHANNELSTATE_HEL_SENT:     break;
            case UA_SECURECHANNELSTATE_HEL_RECEIVED: break;
            case UA_SECURECHANNELSTATE_ACK_SENT:     break;
            case UA_SECURECHANNELSTATE_ACK_RECEIVED: break;
            case UA_SECURECHANNELSTATE_OPN_SENT:     break;
            case UA_SECURECHANNELSTATE_OPEN:         break;
            case UA_SECURECHANNELSTATE_CLOSING:      break;
        }
    }

    if (newSessionState != sessionState) {
        errlogPrintf("open62541session %s: session state changed from %s to %s\n",
            name.c_str(), toStr(sessionState), toStr(newSessionState));
        sessionState = newSessionState;
// TODO: What to do for each sessionState change?
        switch (sessionState) {
            case UA_SESSIONSTATE_CLOSED:             break;
            case UA_SESSIONSTATE_CREATE_REQUESTED:   break;
            case UA_SESSIONSTATE_CREATED:            break;
            case UA_SESSIONSTATE_ACTIVATE_REQUESTED: break;
            case UA_SESSIONSTATE_ACTIVATED:
                //updateNamespaceMap(puasession->getNamespaceTable());
                rebuildNodeIds();
                registerNodes();
                createAllSubscriptions();
                addAllMonitoredItems();
                break;
            case UA_SESSIONSTATE_CLOSING:            break;
        }
    }

/*
    switch (serverStatus) {
        // "The monitoring of the connection to the server detected an error
        // and is trying to reconnect to the server."
    case UaClient::ConnectionErrorApiReconnect:
        // "The server sent a shut-down event and the client API tries a reconnect."
    case UaClient::ServerShutdown:
        // "The connection to the server is deactivated by the user of the client API."
    case UaClient::Disconnected:
        reader.clear();
        writer.clear();
        for (auto it : items) {
            it->setState(ConnectionStatus::down);
            it->setIncomingEvent(ProcessReason::connectionLoss);
        }
        registeredItemsNo = 0;
        break;

        // "The monitoring of the connection to the server indicated
        // a potential connection problem."
    case UaClient::ConnectionWarningWatchdogTimeout:
        break;

        // "The connection to the server is established and is working in normal mode."
    case UaClient::Connected:
        if (serverConnectionStatus == UaClient::Disconnected) {
            updateNamespaceMap(puasession->getNamespaceTable());
            rebuildNodeIds();
            registerNodes();
            createAllSubscriptions();
            addAllMonitoredItems();
        }
        if (serverConnectionStatus != UaClient::ConnectionWarningWatchdogTimeout) {
            if (debug) {
                std::cout << "Session " << name.c_str()
                          << ": triggering initial read for all "
                          << items.size() << " items" << std::endl;
            }
            auto cargo = std::vector<std::shared_ptr<ReadRequest>>(items.size());
            unsigned int i = 0;
            for (auto it : items) {
                it->setState(ConnectionStatus::initialRead);
                cargo[i] = std::make_shared<ReadRequest>();
                cargo[i]->item = it;
                i++;
            }
            // status needs to be updated before requests are being issued
            serverConnectionStatus = serverStatus;
            reader.pushRequest(cargo, menuPriorityHIGH);
        }
        break;

        // "The client was not able to reuse the old session
        // and created a new session during reconnect.
        // This requires to redo register nodes for the new session
        // or to read the namespace array."
    case UaClient::NewSessionCreated:
        updateNamespaceMap(puasession->getNamespaceTable());
        rebuildNodeIds();
        registerNodes();
        createAllSubscriptions();
        addAllMonitoredItems();
        break;
    }
*/
}
/*
void
SessionOpen62541::readComplete (UA_UInt32 transactionId,
                            UA_StatusCode result,
                            const UaDataValues &values,
                            const UaDiagnosticInfos &diagnosticInfos)
{
    Guard G(opslock);
    auto it = outstandingOps.find(transactionId);
    if (it == outstandingOps.end()) {
        errlogPrintf("OPC UA session %s: (readComplete) received a callback "
                     "with unknown transaction id %u - ignored\n",
                     name.c_str(), transactionId);
    } else if (!UA_STATUS_IS_BAD(result)) {
        if (debug >= 2)
            std::cout << "Session " << name.c_str()
                      << ": (readComplete) getting data for read service"
                      << " (transaction id " << transactionId
                      << "; data for " << values.length() << " items)" << std::endl;
        if ((*it->second).size() != values.length())
            errlogPrintf("OPC UA session %s: (readComplete) received a callback "
                         "with %u values for a request containing %lu items\n",
                         name.c_str(), values.length(), (*it->second).size());
        UA_UInt32 i = 0;
        for (auto item : (*it->second)) {
            if (i >= values.length()) {
                item->setIncomingEvent(ProcessReason::readFailure);
            } else {
                if (debug >= 5) {
                    std::cout << "** Session " << name.c_str()
                              << ": (readComplete) getting data for item "
                              << item->getNodeId().toXmlString().toUtf8() << std::endl;
                }
                ProcessReason reason = ProcessReason::readComplete;
                if (OpcUa_IsNotGood(values[i].StatusCode))
                    reason = ProcessReason::readFailure;
                item->setIncomingData(values[i], reason);
            }
            i++;
        }
        outstandingOps.erase(it);
    } else {
        if (debug)
            std::cout << "Session " << name.c_str()
                      << ": (readComplete) for read service"
                      << " (transaction id " << transactionId
                      << ") failed with status " << UA_StatusCode_name(result) << std::endl;
        for (auto item : (*it->second)) {
            if (debug >= 5) {
                std::cout << "** Session " << name.c_str()
                          << ": (readComplete) filing read error (no data) for item "
                          << item->getNodeId().toXmlString().toUtf8() << std::endl;
            }
            item->setIncomingEvent(ProcessReason::readFailure);
            // Not doing initial write if the read has failed
            item->setState(ConnectionStatus::up);
        }
        outstandingOps.erase(it);
    }
}

void
SessionOpen62541::writeComplete (UA_UInt32 transactionId,
                             UA_StatusCode result,
                             const UaStatusCodeArray& results,
                             const UaDiagnosticInfos& diagnosticInfos)
{
    Guard G(opslock);
    auto it = outstandingOps.find(transactionId);
    if (it == outstandingOps.end()) {
        errlogPrintf("OPC UA session %s: (writeComplete) received a callback "
                     "with unknown transaction id %u - ignored\n",
                     name.c_str(), transactionId);
    } else if (!UA_STATUS_IS_BAD(result)) {
        if (debug >= 2)
            std::cout << "Session " << name.c_str()
                      << ": (writeComplete) getting results for write service"
                      << " (transaction id " << transactionId
                      << "; results for " << results.length() << " items)" << std::endl;
        UA_UInt32 i = 0;
        for (auto item : (*it->second)) {
            if (debug >= 5) {
                std::cout << "** Session " << name.c_str()
                          << ": (writeComplete) getting results for item "
                          << item->getNodeId().toXmlString().toUtf8() << std::endl;
            }
            ProcessReason reason = ProcessReason::writeComplete;
            if (UA_STATUS_IS_BAD(results[i]))
                reason = ProcessReason::writeFailure;
            item->setIncomingEvent(reason);
            item->setState(ConnectionStatus::up);
            i++;
        }
        outstandingOps.erase(it);
    } else {
        if (debug)
            std::cout << "Session " << name.c_str()
                      << ": (writeComplete) for write service"
                      << " (transaction id " << transactionId
                      << ") failed with status " << UA_StatusCode_name(result) << std::endl;
        for (auto item : (*it->second)) {
            if (debug >= 5) {
                std::cout << "** Session " << name.c_str()
                          << ": (writeComplete) filing write error for item "
                          << item->getNodeId().toXmlString().toUtf8() << std::endl;
            }
            item->setIncomingEvent(ProcessReason::writeFailure);
            item->setState(ConnectionStatus::up);
        }
        outstandingOps.erase(it);
    }
}
*/

void
SessionOpen62541::showAll (const int level)
{
    unsigned int connected = 0;
    unsigned int subscriptions = 0;
    unsigned long int items = 0;

    for (auto &it : sessions) {
        if (it.second->isConnected()) connected++;
        subscriptions += it.second->noOfSubscriptions();
        items += it.second->noOfItems();
    }
    std::cout << "OPC UA: total of "
              << sessions.size() << " session(s) ("
              << connected << " connected) with "
              << subscriptions << " subscription(s) and "
              << items << " items" << std::endl;
    if (level >= 1) {
        for (auto &it : sessions) {
            it.second->show(level-1);
        }
    }
}

SessionOpen62541::~SessionOpen62541 ()
{
    if (client) {
        Guard G(clientlock);
        // Cancel all asynchronous operations,
        // disconnect and remove all subscriptions.
        // Frees the client memory.
        UA_Client_delete(client);
        client = nullptr;
    }
}

void
SessionOpen62541::initHook (initHookState state)
{
    switch (state) {
    case initHookAfterDatabaseRunning:
    {
        errlogPrintf("OPC UA: Autoconnecting %zu sessions\n", sessions.size());
        for (auto &it : sessions) {
            if (it.second->autoConnect)
                it.second->connect();
        }
        break;
    }
    default:
        break;
    }
}

void
SessionOpen62541::atExit (void *)
{
    errlogPrintf("OPC UA: Disconnecting %zu sessions\n", sessions.size());
    for (auto &it : sessions) {
        it.second->disconnect();
    }
}

} // namespace DevOpcua
