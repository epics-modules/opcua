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

// Avoid problems on Windows (macros min, max clash with numeric_limits<>)
#ifdef _WIN32
#    define NOMINMAX
#endif

#include <iostream>
#include <string>
#include <map>
#include <algorithm>
#include <utility>
#include <vector>
#include <limits>

#include <uaclientsdk.h>
#include <uasession.h>

#include <epicsExit.h>
#include <epicsThread.h>
#include <epicsAtomic.h>
#include <initHooks.h>
#include <errlog.h>

#define epicsExportSharedSymbols
#include "Session.h"
#include "RecordConnector.h"
#include "RequestQueueBatcher.h"
#include "SessionUaSdk.h"
#include "SubscriptionUaSdk.h"
#include "DataElementUaSdk.h"
#include "ItemUaSdk.h"

namespace DevOpcua {

using namespace UaClientSdk;

static epicsThreadOnceId session_uasdk_ihooks_once = EPICS_THREAD_ONCE_INIT;
static epicsThreadOnceId session_uasdk_atexit_once = EPICS_THREAD_ONCE_INIT;

std::map<std::string, SessionUaSdk*> SessionUaSdk::sessions;

// Cargo structure and batcher for write requests
struct WriteRequest {
    ItemUaSdk *item;
    OpcUa_WriteValue wvalue;
};

// Cargo structure and batcher for read requests
struct ReadRequest {
    ItemUaSdk *item;
};

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
    epicsAtExit(SessionUaSdk::atExit, nullptr);
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
    : Session(debug)
    , name(name)
    , serverURL(serverUrl.c_str())
    , autoConnect(autoConnect)
    , registeredItemsNo(0)
    , puasession(new UaSession())
    , serverConnectionStatus(UaClient::Disconnected)
    , transactionId(0)
    , writer("OPCwr-" + name, *this, batchNodes)
    , writeNodesMax(0)
    , writeTimeoutMin(0)
    , writeTimeoutMax(0)
    , reader("OPCrd-" + name, *this, batchNodes)
    , readNodesMax(0)
    , readTimeoutMin(0)
    , readTimeoutMax(0)
{
    int status;
    char host[256] = { 0 };

    status = gethostname(host, sizeof(host));
    if (status) strcpy(host, "unknown-host");

    //TODO: allow overriding by env variable
    connectInfo.sApplicationName = "EPICS IOC";
    connectInfo.sApplicationUri  = UaString("urn:%1:EPICS:IOC").arg(host);
    connectInfo.sProductUri      = "urn:EPICS:IOC";
    connectInfo.sSessionName     = UaString(name.c_str());

    connectInfo.bAutomaticReconnect = autoConnect;
    connectInfo.bRetryInitialConnect = autoConnect;
    connectInfo.nMaxOperationsPerServiceCall = batchNodes;

    connectInfo.typeDictionaryMode = UaClientSdk::UaClient::ReadTypeDictionaries_Reconnect;

    //TODO: init security settings
    if ((clientCertificate && (clientCertificate[0] != '\0'))
            || (clientPrivateKey && (clientPrivateKey[0] != '\0')))
        errlogPrintf("OPC UA security not supported yet\n");

    sessions[name] = this;
    epicsThreadOnce(&DevOpcua::session_uasdk_ihooks_once, &DevOpcua::session_uasdk_ihooks_register, nullptr);
}

const std::string &
SessionUaSdk::getName () const
{
    return name;
}

OpcUa_UInt32
SessionUaSdk::getTransactionId ()
{
    return static_cast<OpcUa_UInt32>(epics::atomic::increment(transactionId));
}

SessionUaSdk &
SessionUaSdk::findSession (const std::string &name)
{
    auto it = sessions.find(name);
    if (it == sessions.end()) {
        throw std::runtime_error("no such session");
    }
    return *(it->second);
}

bool
SessionUaSdk::sessionExists (const std::string &name)
{
    auto it = sessions.find(name);
    return !(it == sessions.end());
}

void
SessionUaSdk::setOption (const std::string &name, const std::string &value)
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
        connectInfo.nMaxOperationsPerServiceCall = ul;
        updateReadBatcher = true;
        updateWriteBatcher = true;
    } else if (name == "nodes-max") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        connectInfo.nMaxOperationsPerServiceCall = ul;
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
    if (connectInfo.nMaxOperationsPerServiceCall > 0 && readNodesMax > 0) {
        max = std::min<unsigned int>(connectInfo.nMaxOperationsPerServiceCall, readNodesMax);
    } else {
        max = connectInfo.nMaxOperationsPerServiceCall + readNodesMax;
    }
    if (updateReadBatcher) reader.setParams(max, readTimeoutMin, readTimeoutMax);

    if (connectInfo.nMaxOperationsPerServiceCall > 0 && writeNodesMax > 0) {
        max = std::min<unsigned int>(connectInfo.nMaxOperationsPerServiceCall, writeNodesMax);
    } else {
        max = connectInfo.nMaxOperationsPerServiceCall + writeNodesMax;
    }
    if (updateWriteBatcher) writer.setParams(max, writeTimeoutMin, writeTimeoutMax);
}

long
SessionUaSdk::connect ()
{
    if (!puasession) {
        std::cerr << "Session " << name.c_str()
                  << ": invalid session, cannot connect" << std::endl;
        return -1;
    }
    if (isConnected()) {
        if (debug) std::cerr << "Session " << name.c_str() << ": already connected ("
                             << serverStatusString(serverConnectionStatus) << ")" << std::endl;
        return 0;
    } else {
        UaStatus result = puasession->connect(serverURL,      // URL of the Endpoint
                                              connectInfo,    // General connection settings
                                              securityInfo,   // Security settings
                                              this);          // Callback interface

        if (result.isGood()) {
            if (debug) std::cerr << "Session " << name.c_str()
                                 << ": connect service ok" << std::endl;
        } else {
            std::cerr << "Session " << name.c_str()
                      << ": connect service failed with status "
                      << result.toString().toUtf8() << std::endl;
        }
        // asynchronous: remaining actions are done on the status-change callback
        return !result.isGood();
    }
}

long
SessionUaSdk::disconnect ()
{
    if (isConnected()) {
        ServiceSettings serviceSettings;

        UaStatus result = puasession->disconnect(serviceSettings,  // Use default settings
                                                 OpcUa_True);      // Delete subscriptions

        if (result.isGood()) {
            if (debug) std::cerr << "Session " << name.c_str()
                                 << ": disconnect service ok" << std::endl;
        } else {
            std::cerr << "Session " << name.c_str()
                      << ": disconnect service failed with status "
                      << result.toString().toUtf8() << std::endl;
        }
        // Detach all subscriptions of this session from driver
        for (auto &it : subscriptions) {
            it.second->clear();
        }
        return !result.isGood();
    } else {
        if (debug) std::cerr << "Session " << name.c_str() << ": already disconnected ("
                             << serverStatusString(serverConnectionStatus) << ")" << std::endl;
        return 0;
    }
}

bool
SessionUaSdk::isConnected () const
{
    if (puasession)
        return (!!puasession->isConnected()
                && serverConnectionStatus != UaClient::ConnectionErrorApiReconnect);
    else
        return false;
}

void
SessionUaSdk::requestRead (ItemUaSdk &item)
{
    auto cargo = std::make_shared<ReadRequest>();
    cargo->item = &item;
    reader.pushRequest(cargo, item.recConnector->getRecordPriority());
}

// Low level reader function called by the RequestQueueBatcher
void
SessionUaSdk::processRequests (std::vector<std::shared_ptr<ReadRequest>> &batch)
{
    UaStatus status;
    UaReadValueIds nodesToRead;
    std::unique_ptr<std::vector<ItemUaSdk *>> itemsToRead(new std::vector<ItemUaSdk *>);
    ServiceSettings serviceSettings;
    OpcUa_UInt32 id = getTransactionId();

    nodesToRead.create(static_cast<OpcUa_UInt32>(batch.size()));
    OpcUa_UInt32 i = 0;
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

	    if (status.isBad()) {
	        errlogPrintf("OPC UA session %s: (requestRead) beginRead service failed with status %s\n",
	                     name.c_str(), status.toString().toUtf8());
            //TODO: create writeFailure events for all items of the batch
//	        item.setIncomingEvent(ProcessReason::readFailure);

        } else {
            if (debug >= 5)
                std::cout << "Session " << name.c_str()
                          << ": (requestRead) beginRead service ok"
                          << " (transaction id " << id
                          << "; retrieving " << nodesToRead.length() << " nodes)" << std::endl;
            outstandingOps.insert(std::pair<OpcUa_UInt32, std::unique_ptr<std::vector<ItemUaSdk *>>>
                                  (id, std::move(itemsToRead)));
        }
    }
}

void
SessionUaSdk::requestWrite (ItemUaSdk &item)
{
    auto cargo = std::make_shared<WriteRequest>();
    cargo->item = &item;
    item.getOutgoingData().copyTo(&cargo->wvalue.Value.Value);
    item.clearOutgoingData();
    writer.pushRequest(cargo, item.recConnector->getRecordPriority());
}

// Low level writer function called by the RequestQueueBatcher
void
SessionUaSdk::processRequests (std::vector<std::shared_ptr<WriteRequest>> &batch)
{
    UaStatus status;
    UaWriteValues nodesToWrite;
    std::unique_ptr<std::vector<ItemUaSdk *>> itemsToWrite(new std::vector<ItemUaSdk *>);
    ServiceSettings serviceSettings;
    OpcUa_UInt32 id = getTransactionId();

    nodesToWrite.create(static_cast<OpcUa_UInt32>(batch.size()));
    OpcUa_UInt32 i = 0;
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

	    if (status.isBad()) {
	        errlogPrintf("OPC UA session %s: (requestWrite) beginWrite service failed with status %s\n",
	                     name.c_str(), status.toString().toUtf8());
            //TODO: create writeFailure events for all items of the batch
//	        item.setIncomingEvent(ProcessReason::writeFailure);

        } else {
            if (debug >= 5)
                std::cout << "Session " << name.c_str()
                          << ": (requestWrite) beginWrite service ok"
                          << " (transaction id " << id
                          << "; writing " << nodesToWrite.length() << " nodes)" << std::endl;
            outstandingOps.insert(std::pair<OpcUa_UInt32, std::unique_ptr<std::vector<ItemUaSdk *>>>
                                  (id, std::move(itemsToWrite)));
        }
    }
}

void
SessionUaSdk::createAllSubscriptions ()
{
    for (auto &it : subscriptions) {
        it.second->create();
    }
}

void
SessionUaSdk::addAllMonitoredItems ()
{
    for (auto &it : subscriptions) {
        it.second->addMonitoredItems();
    }
}

void
SessionUaSdk::registerNodes ()
{
    UaStatus          status;
    UaNodeIdArray     nodesToRegister;
    UaNodeIdArray     registeredNodes;
    ServiceSettings   serviceSettings;

    nodesToRegister.create(static_cast<OpcUa_UInt32>(items.size()));
    OpcUa_UInt32 i = 0;
    for (auto &it : items) {
        if (it->linkinfo.registerNode) {
            it->getNodeId().copyTo(&nodesToRegister[i]);
            i++;
        }
    }
    nodesToRegister.resize(i);
    registeredItemsNo = i;

    if (registeredItemsNo) {
        status = puasession->registerNodes(serviceSettings,     // Use default settings
                                           nodesToRegister,     // Array of nodeIds to register
                                           registeredNodes);    // Returns an array of registered nodeIds

        if (status.isBad()) {
            errlogPrintf("OPC UA session %s: (registerNodes) registerNodes service failed with status %s\n",
                         name.c_str(), status.toString().toUtf8());
        } else {
            if (debug)
                std::cout << "Session " << name.c_str()
                          << ": (registerNodes) registerNodes service ok"
                          << " (" << registeredNodes.length() << " nodes registered)" << std::endl;
            i = 0;
            for (auto &it : items) {
                if (it->linkinfo.registerNode) {
                    it->setRegisteredNodeId(registeredNodes[i]);
                    i++;
                }
            }
            registeredItemsNo = i;
        }
    }
}

void
SessionUaSdk::rebuildNodeIds ()
{
    for (auto &it : items)
        it->rebuildNodeId();
}

/* Add a mapping to the session's map, replacing any existing mappings with the same
 * index or URI */
void
SessionUaSdk::addNamespaceMapping (const OpcUa_UInt16 nsIndex, const std::string &uri)
{
    for (auto &it : namespaceMap) {
        if (it.second == nsIndex) {
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
void
SessionUaSdk::updateNamespaceMap(const UaStringArray &nsArray)
{
    if (debug)
        std::cout << "Session " << name.c_str()
                  << ": (updateNamespaceMap) namespace array with " << nsArray.length()
                  << " elements read; updating index map with " << namespaceMap.size()
                  << " entries" << std::endl;
    if (namespaceMap.size()) {
        nsIndexMap.clear();
        for (OpcUa_UInt16 i = 0; i < nsArray.length(); i++) {
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

void
SessionUaSdk::show (const int level) const
{
    std::cout << "session="      << name
              << " url="         << serverURL.toUtf8()
              << " status="      << serverStatusString(serverConnectionStatus)
              << " cert="        << "[none]"
              << " key="         << "[none]"
              << " debug="       << debug
              << " batch=";
    if (isConnected())
        std::cout << puasession->maxOperationsPerServiceCall();
    else
        std::cout << "?";
    std::cout << "(" << connectInfo.nMaxOperationsPerServiceCall << ")"
              << " autoconnect=" << (connectInfo.bAutomaticReconnect ? "y" : "n")
              << " items=" << items.size()
              << " registered=" << registeredItemsNo
              << " subscriptions=" << subscriptions.size()
              << " reader=" << reader.maxRequests() << "/"
              << reader.minHoldOff() << "-" << reader.maxHoldOff() << "ms"
              << " writer=" << writer.maxRequests() << "/"
              << writer.minHoldOff() << "-" << writer.maxHoldOff() << "ms"
              << std::endl;

    if (level >= 3) {
        if (namespaceMap.size()) {
            std::cout << "Configured Namespace Mapping "
                      << "(local -> Namespace URI -> server)" << std::endl;
            for (auto p : namespaceMap) {
                std::cout << " " << p.second << " -> " << p.first << " -> "
                          << mapNamespaceIndex(p.second) << std::endl;
            }
        }
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
SessionUaSdk::addItemUaSdk (ItemUaSdk *item)
{
    items.push_back(item);
}

void
SessionUaSdk::removeItemUaSdk (ItemUaSdk *item)
{
    auto it = std::find(items.begin(), items.end(), item);
    if (it != items.end())
        items.erase(it);
}

OpcUa_UInt16
SessionUaSdk::mapNamespaceIndex (const OpcUa_UInt16 nsIndex) const
{
    OpcUa_UInt16 serverIndex = nsIndex;
    if (nsIndexMap.size()) {
        auto it = nsIndexMap.find(nsIndex);
        if (it != nsIndexMap.end())
            serverIndex = it->second;
    }
    return serverIndex;
}

// UaSessionCallback interface

void SessionUaSdk::connectionStatusChanged (
    OpcUa_UInt32             clientConnectionId,
    UaClient::ServerStatus   serverStatus)
{
    OpcUa_ReferenceParameter(clientConnectionId);
    errlogPrintf("OPC UA session %s: connection status changed from %s to %s\n",
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
    serverConnectionStatus = serverStatus;
}

void
SessionUaSdk::readComplete (OpcUa_UInt32 transactionId,
                            const UaStatus &result,
                            const UaDataValues &values,
                            const UaDiagnosticInfos &diagnosticInfos)
{
    Guard G(opslock);
    auto it = outstandingOps.find(transactionId);
    if (it == outstandingOps.end()) {
        errlogPrintf("OPC UA session %s: (readComplete) received a callback "
                     "with unknown transaction id %u - ignored\n",
                     name.c_str(), transactionId);
    } else if (result.isGood()) {
        if (debug >= 2)
            std::cout << "Session " << name.c_str()
                      << ": (readComplete) getting data for read service"
                      << " (transaction id " << transactionId
                      << "; data for " << values.length() << " items)" << std::endl;
        if ((*it->second).size() != values.length())
            errlogPrintf("OPC UA session %s: (readComplete) received a callback "
                         "with %u values for a request containing %lu items\n",
                         name.c_str(), values.length(), (*it->second).size());
        OpcUa_UInt32 i = 0;
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
                      << ") failed with status " << result.toString() << std::endl;
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
SessionUaSdk::writeComplete (OpcUa_UInt32 transactionId,
                             const UaStatus&          result,
                             const UaStatusCodeArray& results,
                             const UaDiagnosticInfos& diagnosticInfos)
{
    Guard G(opslock);
    auto it = outstandingOps.find(transactionId);
    if (it == outstandingOps.end()) {
        errlogPrintf("OPC UA session %s: (writeComplete) received a callback "
                     "with unknown transaction id %u - ignored\n",
                     name.c_str(), transactionId);
    } else if (result.isGood()) {
        if (debug >= 2)
            std::cout << "Session " << name.c_str()
                      << ": (writeComplete) getting results for write service"
                      << " (transaction id " << transactionId
                      << "; results for " << results.length() << " items)" << std::endl;
        OpcUa_UInt32 i = 0;
        for (auto item : (*it->second)) {
            if (debug >= 5) {
                std::cout << "** Session " << name.c_str()
                          << ": (writeComplete) getting results for item "
                          << item->getNodeId().toXmlString().toUtf8() << std::endl;
            }
            ProcessReason reason = ProcessReason::writeComplete;
            if (OpcUa_IsBad(results[i]))
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
                      << ") failed with status " << result.toString() << std::endl;
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

void
SessionUaSdk::showAll (const int level)
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
        errlogPrintf("OPC UA: Autoconnecting sessions\n");
        for (auto &it : sessions) {
            if (it.second->autoConnect)
                it.second->connect();
        }
        epicsThreadOnce(&DevOpcua::session_uasdk_atexit_once, &DevOpcua::session_uasdk_atexit_register, nullptr);
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
    errlogPrintf("OPC UA: Disconnecting sessions\n");
    for (auto &it : sessions) {
        it.second->disconnect();
    }
}

} // namespace DevOpcua
