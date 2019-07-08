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
#  define NOMINMAX
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
#include "SessionUaSdk.h"
#include "SubscriptionUaSdk.h"
#include "DataElementUaSdk.h"
#include "ItemUaSdk.h"

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
    if (name == "clientcert") {
        errlogPrintf("security not implemented\n");
    } else if (name == "clientkey") {
        errlogPrintf("security not implemented\n");
    } else if (name == "batch-nodes") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        connectInfo.nMaxOperationsPerServiceCall = ul;
    } else {
        errlogPrintf("unknown option '%s' ignored\n", name.c_str());
    }
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
        UaStatus result = puasession->connect(serverURL,      // URL of the Endpoint
                                              connectInfo,    // General connection settings
                                              securityInfo,   // Security settings
                                              this);          // Callback interface

        if (result.isGood()) {
            if (debug) std::cerr << "OPC UA session " << name.c_str()
                                 << ": connect service ok" << std::endl;
        } else {
            std::cerr << "OPC UA session " << name.c_str()
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
            if (debug) std::cerr << "OPC UA session " << name.c_str()
                                 << ": disconnect service ok" << std::endl;
        } else {
            std::cerr << "OPC UA session " << name.c_str()
                      << ": disconnect service failed with status "
                      << result.toString().toUtf8() << std::endl;
        }
        // Detach all subscriptions of this session from driver
        for (auto &it : subscriptions) {
            it.second->clear();
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
        return (!!puasession->isConnected()
                && serverConnectionStatus != UaClient::ConnectionErrorApiReconnect);
    else
        return false;
}

void
SessionUaSdk::readBatchOfNodes (std::vector<ItemUaSdk *>::iterator from,
                                std::vector<ItemUaSdk *>::iterator to)
{
    UaStatus status;
    UaReadValueIds nodesToRead;
    std::unique_ptr<std::vector<ItemUaSdk *>> itemsToRead(new std::vector<ItemUaSdk *>);
    ServiceSettings serviceSettings;
    OpcUa_UInt32 id = getTransactionId();

    nodesToRead.create(static_cast<OpcUa_UInt32>(to - from));
    OpcUa_UInt32 i = 0;
    for (auto it = from; it != to; it++) {
        (*it)->getNodeId().copyTo(&nodesToRead[i].NodeId);
        nodesToRead[i].AttributeId = OpcUa_Attributes_Value;
        i++;
        itemsToRead->push_back(*it);
    }

    Guard G(opslock);
    status = puasession->beginRead(serviceSettings,                // Use default settings
                                   0,                              // Max age
                                   OpcUa_TimestampsToReturn_Both,  // Time stamps to return
                                   nodesToRead,                    // Array of nodes to read
                                   id);                            // Transaction id

    if (status.isBad()) {
        errlogPrintf("OPC UA session %s: (readBatchOfNodes) beginRead service failed with status %s\n",
                     name.c_str(), status.toString().toUtf8());
    } else {
        if (debug)
            std::cout << "OPC UA session " << name.c_str()
                      << ": (readBatchOfNodes) beginRead service ok"
                      << " (transaction id " << id
                      << "; retrieving " << nodesToRead.length() << " nodes)" << std::endl;
        outstandingOps.insert(std::pair<OpcUa_UInt32, std::unique_ptr<std::vector<ItemUaSdk *>>>
                              (id, std::move(itemsToRead)));
    }
}

//TODO: Push to queue for worker thread (instead of doing an explicit request)
void
SessionUaSdk::readAllNodes ()
{
    OpcUa_UInt32 numberPerBatch = connectInfo.nMaxOperationsPerServiceCall;

    if (numberPerBatch == 0 || items.size() <= numberPerBatch) {
        readBatchOfNodes(items.begin(), items.end());
    } else {
        std::vector<ItemUaSdk *>::iterator it;
        for (it = items.begin(); (items.end() - it) > numberPerBatch; it += numberPerBatch) {
            readBatchOfNodes(it, it + numberPerBatch);
        }
        if (it < items.end()) readBatchOfNodes(it, items.end());
    }
}

//TODO: Push to queue for worker thread (instead of doing a single item request)
//      remember to apply connectInfo.nMaxOperationsPerServiceCall
void
SessionUaSdk::requestRead (ItemUaSdk &item)
{
    UaStatus status;
    UaReadValueIds nodesToRead;
    std::unique_ptr<std::vector<ItemUaSdk *>> itemsToRead(new std::vector<ItemUaSdk *>);
    ServiceSettings serviceSettings;
    OpcUa_UInt32 id = getTransactionId();

    nodesToRead.create(1);
    item.getNodeId().copyTo(&nodesToRead[0].NodeId);
    nodesToRead[0].AttributeId = OpcUa_Attributes_Value;
    itemsToRead->push_back(&item);

    Guard G(opslock);
    status = puasession->beginRead(serviceSettings,                // Use default settings
                                   0,                              // Max age
                                   OpcUa_TimestampsToReturn_Both,  // Time stamps to return
                                   nodesToRead,                    // Array of nodes to read
                                   id);                            // Transaction id

    if (status.isBad()) {
        errlogPrintf("OPC UA session %s: (requestRead) beginRead service failed with status %s\n",
                     name.c_str(), status.toString().toUtf8());
        item.setReadStatus(status.code());
        item.setIncomingEvent(ProcessReason::readFailure);

    } else {
        if (debug)
            std::cout << "Session " << name.c_str()
                      << ": (requestRead) beginRead service ok"
                      << " (transaction id " << id
                      << "; retrieving " << nodesToRead.length() << " nodes)" << std::endl;
        outstandingOps.insert(std::pair<OpcUa_UInt32, std::unique_ptr<std::vector<ItemUaSdk *>>>
                              (id, std::move(itemsToRead)));
    }
}

//TODO: Push to queue for worker thread (instead of doing a single item request)
//      remember to apply connectInfo.nMaxOperationsPerServiceCall
void
SessionUaSdk::requestWrite (ItemUaSdk &item)
{
    UaStatus status;
    UaWriteValues nodesToWrite;
    std::unique_ptr<std::vector<ItemUaSdk *>> itemsToWrite(new std::vector<ItemUaSdk *>);
    ServiceSettings serviceSettings;
    OpcUa_UInt32 id = getTransactionId();

    nodesToWrite.create(1);
    item.getNodeId().copyTo(&nodesToWrite[0].NodeId);
    nodesToWrite[0].AttributeId = OpcUa_Attributes_Value;
    item.getOutgoingData().copyTo(&nodesToWrite[0].Value.Value);
    item.clearOutgoingData();
    itemsToWrite->push_back(&item);

    Guard G(opslock);
    status = puasession->beginWrite(serviceSettings,        // Use default settings
                                    nodesToWrite,           // Array of nodes/data to write
                                    id);                    // Transaction id

    if (status.isBad()) {
        errlogPrintf("OPC UA session %s: (requestWrite) beginWrite service failed with status %s\n",
                     name.c_str(), status.toString().toUtf8());
        item.setWriteStatus(status.code());
        item.setIncomingEvent(ProcessReason::writeFailure);

    } else {
        if (debug)
            std::cout << "Session " << name.c_str()
                      << ": (requestWrite) beginWrite service ok"
                      << " (transaction id " << id
                      << "; writing " << nodesToWrite.length() << " nodes)" << std::endl;
        outstandingOps.insert(std::pair<OpcUa_UInt32, std::unique_ptr<std::vector<ItemUaSdk *>>>
                              (id, std::move(itemsToWrite)));
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

    status = puasession->registerNodes(serviceSettings,     // Use default settings
                                       nodesToRegister,     // Array of nodeIds to register
                                       registeredNodes);    // Returns an array of registered nodeIds

    if (status.isBad()) {
        errlogPrintf("OPC UA session %s: (registerNodes) registerNodes service failed with status %s\n",
                     name.c_str(), status.toString().toUtf8());
    } else {
        if (debug)
            std::cout << "OPC UA session " << name.c_str()
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

void
SessionUaSdk::rebuildNodeIds ()
{
    for (auto &it : items)
        it->rebuildNodeId();
    registeredItemsNo = 0;
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
              << std::endl;

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
        for (auto &it : items)
            it->setIncomingEvent(ProcessReason::connectionLoss);
        break;

        // "The monitoring of the connection to the server indicated
        // a potential connection problem."
    case UaClient::ConnectionWarningWatchdogTimeout:
        break;

        // "The connection to the server is established and is working in normal mode."
    case UaClient::Connected:
        readAllNodes();
        if (serverConnectionStatus == UaClient::Disconnected) {
            registerNodes();
            createAllSubscriptions();
            addAllMonitoredItems();
        }
        break;

        // "The client was not able to reuse the old session
        // and created a new session during reconnect.
        // This requires to redo register nodes for the new session
        // or to read the namespace array."
    case UaClient::NewSessionCreated:
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
    } else {
        if (debug)
            std::cout << "Session " << name.c_str()
                      << ": (readComplete) getting data for read service"
                      << " (transaction id " << transactionId
                      << "; data for " << values.length() << " items)" << std::endl;
        OpcUa_UInt32 i = 0;
        for (auto item : (*it->second)) {
            if (debug >= 5) {
                std::cout << "** Session " << name.c_str()
                          << ": (readComplete) getting data for item "
                          << item->getNodeId().toXmlString().toUtf8() << std::endl;
            }
            item->setReadStatus(values[i].StatusCode);
            item->setIncomingData(values[i], ProcessReason::readComplete);
            i++;
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
    } else {
        if (debug)
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
            item->setWriteStatus(results[i]);
            item->setIncomingEvent(ProcessReason::writeComplete);
            i++;
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
