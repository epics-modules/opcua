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

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>
#include <algorithm>
#include <utility>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <winsock2.h>
#endif

#include <uaplatformlayer.h>
#include <uaclientsdk.h>
#include <uasession.h>
#include <uadiscovery.h>
#include <uadir.h>
#include <uaenumdefinition.h>

#ifdef HAS_SECURITY
#include <uapkicertificate.h>
#include <uapkirsakeypair.h>
#endif

#include <epicsExit.h>
#include <epicsThread.h>
#include <epicsAtomic.h>
#include <initHooks.h>
#include <errlog.h>

#define epicsExportSharedSymbols
#include "Session.h"
#include "RecordConnector.h"
#include "linkParser.h"
#include "RequestQueueBatcher.h"
#include "SessionUaSdk.h"
#include "SubscriptionUaSdk.h"
#include "DataElementUaSdkNode.h"
#include "ItemUaSdk.h"

namespace DevOpcua {

using namespace UaClientSdk;

static epicsThreadOnceId session_uasdk_ihooks_once = EPICS_THREAD_ONCE_INIT;
static epicsThreadOnceId session_uasdk_atexit_once = EPICS_THREAD_ONCE_INIT;

Registry<SessionUaSdk> SessionUaSdk::sessions;

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
    }
    return "";
}

inline const char *
securityModeString (const OpcUa_MessageSecurityMode mode)
{
    switch (mode) {
    case OpcUa_MessageSecurityMode_Invalid:         return "Invalid";
    case OpcUa_MessageSecurityMode_None:            return "None";
    case OpcUa_MessageSecurityMode_Sign:            return "Sign";
    case OpcUa_MessageSecurityMode_SignAndEncrypt:  return "SignAndEncrypt";
    default:                                        return "<unknown>";
    }
}

inline const char *
reqSecurityModeString(const RequestedSecurityMode mode)
{
    switch (mode) {
    case RequestedSecurityMode::Best:            return "best";
    case RequestedSecurityMode::None:            return "None";
    case RequestedSecurityMode::Sign:            return "Sign";
    case RequestedSecurityMode::SignAndEncrypt:  return "SignAndEncrypt";
    default:                                     return "<unknown>";
    }
}

inline OpcUa_MessageSecurityMode
OpcUaSecurityMode(const RequestedSecurityMode mode)
{
    switch (mode) {
    case RequestedSecurityMode::Best:            return OpcUa_MessageSecurityMode_Invalid;
    case RequestedSecurityMode::None:            return OpcUa_MessageSecurityMode_None;
    case RequestedSecurityMode::Sign:            return OpcUa_MessageSecurityMode_Sign;
    case RequestedSecurityMode::SignAndEncrypt:  return OpcUa_MessageSecurityMode_SignAndEncrypt;
    default:                                     return OpcUa_MessageSecurityMode_Invalid;
    }
}

inline const char *
SessionUaSdk::connectResultString (const ConnectResult result)
{
    switch (result) {
    case ConnectResult::fatal:               return "fatal";
    case ConnectResult::ok:                  return "ok";
    case ConnectResult::cantConnect:         return "cantConnect";
    case ConnectResult::noMatchingEndpoint:  return "noMatchingEndpoint";
    }
    return "";
}

SessionUaSdk::SessionUaSdk(const std::string &name,
                           const std::string &serverUrl)
    : Session(name)
    , serverURL(serverUrl.c_str())
    , registeredItemsNo(0)
    , puasession(new UaSession())
    , reqSecurityMode(RequestedSecurityMode::Best)
    , reqSecurityPolicyURI("http://opcfoundation.org/UA/SecurityPolicy#None")
    , serverConnectionStatus(UaClient::Disconnected)
    , transactionId(0)
    , writer("OPCwr-" + name, *this)
    , writeNodesMax(0)
    , writeTimeoutMin(0)
    , writeTimeoutMax(0)
    , reader("OPCrd-" + name, *this)
    , readNodesMax(0)
    , readTimeoutMin(0)
    , readTimeoutMax(0)
{
    //TODO: allow overriding by env variable
    connectInfo.sApplicationName = "EPICS IOC";
    connectInfo.sApplicationUri  = UaString(applicationUri.c_str());
    connectInfo.sProductUri      = "urn:EPICS:IOC";
    connectInfo.sSessionName     = UaString(name.c_str());

    connectInfo.bAutomaticReconnect = autoConnect;
    connectInfo.bRetryInitialConnect = autoConnect;
    connectInfo.nMaxOperationsPerServiceCall = 0;

    connectInfo.typeDictionaryMode = UaClientSdk::UaClient::ReadTypeDictionaries_Reconnect;

    sessions.insert({name, this});
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

void
SessionUaSdk::setOption (const std::string &name, const std::string &value)
{
    bool updateReadBatcher = false;
    bool updateWriteBatcher = false;

    if (debug || name == "debug")
        std::cerr << "Session " << this->name << ": setting option " << name << " to " << value
                  << std::endl;

    if (name == "sec-mode") {
        if (value == "best") {
            reqSecurityMode = RequestedSecurityMode::Best;
        } else if (value == "None") {
            reqSecurityMode = RequestedSecurityMode::None;
        } else if (value == "SignAndEncrypt") {
            reqSecurityMode = RequestedSecurityMode::SignAndEncrypt;
        } else if (value == "Sign") {
            reqSecurityMode = RequestedSecurityMode::Sign;
        } else {
            errlogPrintf("invalid security mode (valid: best None Sign SignAndEncrypt)\n");
        }
    } else if (name == "sec-policy") {
        bool found = false;
        for (const auto &p : securitySupportedPolicies) {
            if (value == p.second) {
                found = true;
                reqSecurityPolicyURI = UaString(p.first.c_str());
            }
        }
        if (!found) {
            std::string s;
            for (const auto &p : securitySupportedPolicies)
                s += " " + p.second;
            errlogPrintf("invalid security policy (valid:%s)\n", s.c_str());
        }
    } else if (name == "sec-id") {
        securityIdentityFile = value;
    } else if (name == "debug") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        debug = ul;
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
    } else if (name == "autoconnect") {
        if (value.length() > 0)
            autoConnect = getYesNo(value[0]);
    } else {
        errlogPrintf("unknown option '%s' - ignored\n", name.c_str());
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
SessionUaSdk::connect(bool manual)
{
    if (!puasession) {
        std::cerr << "Session " << name.c_str() << ": invalid session, cannot connect" << std::endl;
        return -1;
    }

    if (isConnected()) {
        if (debug || manual)
            std::cerr << "Session " << name.c_str() << ": already connected ("
                      << serverStatusString(serverConnectionStatus) << ")" << std::endl;
        return 0;
    }

    disconnect(); // Do a proper disconnection before attempting to reconnect

    setupClientSecurityInfo(securityInfo, &name, debug);

    ConnectResult secResult = setupSecurity();
    if (secResult) {
        if (manual || debug)
            errlogPrintf("OPC UA session %s: security discovery and setup failed with status %s\n",
                         name.c_str(),
                         connectResultString(secResult));
        if (autoConnect)
            autoConnector.start();
        return -1;
    }

    UaStatus result = puasession->connect(serverURL,    // URL of the Endpoint
                                          connectInfo,  // General connection settings
                                          securityInfo, // Security settings
                                          this);        // Callback interface

    if (result.isGood()) {
        if (debug)
            std::cerr << "Session " << name.c_str() << ": connect service succeeded" << std::endl;
    } else {
        if (manual || debug)
            errlogPrintf("OPC UA session %s: connect service failed with status %s\n",
                         name.c_str(),
                         result.toString().toUtf8());
        if (autoConnect)
            autoConnector.start();
    }
    // asynchronous: remaining actions are done on the status-change callback
    return !result.isGood();
}

long
SessionUaSdk::disconnect()
{
    if (serverConnectionStatus != UaClient::Disconnected) {
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
        if (debug)
            std::cerr << "Session " << name.c_str() << ": (disconnect) already disconnected ("
                      << serverStatusString(serverConnectionStatus) << ")" << std::endl;
        return 0;
    }
}

bool
SessionUaSdk::isConnected() const
{
    return (serverConnectionStatus == UaClient::ConnectionWarningWatchdogTimeout
            || serverConnectionStatus == UaClient::Connected);
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
SessionUaSdk::processRequests(std::vector<std::shared_ptr<ReadRequest>> &batch)
{
    if (!isConnected())
        return;

    UaStatus status;
    UaReadValueIds nodesToRead;
    std::unique_ptr<std::vector<ItemUaSdk *>> itemsToRead(new std::vector<ItemUaSdk *>);
    ServiceSettings serviceSettings;
    OpcUa_UInt32 id = getTransactionId();

    nodesToRead.create(static_cast<OpcUa_UInt32>(batch.size() * no_of_properties_read));
    OpcUa_UInt32 i = 0;
    for (auto c : batch) {
        c->item->getNodeId().copyTo(&nodesToRead[i].NodeId);
        nodesToRead[i].AttributeId = OpcUa_Attributes_DataType;
        i++;
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
            errlogPrintf(
                "OPC UA session %s: (requestRead) beginRead service failed with status %s\n",
                name.c_str(),
                status.toString().toUtf8());
            //TODO: create writeFailure events for all items of the batch
            //	    item.setIncomingEvent(ProcessReason::readFailure);
        } else {
            if (debug >= 5)
                std::cout << "Session " << name.c_str() << ": (requestRead) beginRead service ok"
                          << " (transaction id " << id << "; retrieving " << nodesToRead.length()
                          << " nodes)" << std::endl;
            outstandingOps.insert(
                std::pair<OpcUa_UInt32,
                          std::unique_ptr<std::vector<ItemUaSdk *>>>(id, std::move(itemsToRead)));
        }
    }
}

void
SessionUaSdk::requestWrite (ItemUaSdk &item)
{
    auto cargo = std::make_shared<WriteRequest>();
    cargo->item = &item;
    item.copyAndClearOutgoingData(cargo->wvalue);
    writer.pushRequest(cargo, item.recConnector->getRecordPriority());
}

// Low level writer function called by the RequestQueueBatcher
void
SessionUaSdk::processRequests(std::vector<std::shared_ptr<WriteRequest>> &batch)
{
    if (!isConnected())
        return;

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
        status = puasession->beginWrite(serviceSettings, // Use default settings
                                        nodesToWrite,    // Array of nodes/data to write
                                        id);             // Transaction id

        if (status.isBad()) {
            errlogPrintf(
                "OPC UA session %s: (requestWrite) beginWrite service failed with status %s\n",
                name.c_str(),
                status.toString().toUtf8());
            //TODO: create writeFailure events for all items of the batch
            //	    item.setIncomingEvent(ProcessReason::writeFailure);
        } else {
            if (debug >= 5)
                std::cout << "Session " << name.c_str() << ": (requestWrite) beginWrite service ok"
                          << " (transaction id " << id << "; writing " << nodesToWrite.length()
                          << " nodes)" << std::endl;
            outstandingOps.insert(
                std::pair<OpcUa_UInt32,
                          std::unique_ptr<std::vector<ItemUaSdk *>>>(id, std::move(itemsToWrite)));
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
SessionUaSdk::showSecurity ()
{
    UaStatus status;
    ServiceSettings serviceSettings;
    UaApplicationDescriptions applicationDescriptions;
    UaDiscovery discovery;

    status = discovery.findServers(serviceSettings, serverURL, securityInfo, applicationDescriptions);
    if (status.isBad()) {
        std::cerr << "Session " << name << ": (showSecurity) UaDiscovery::findServers failed"
                  << " with status " << status.toString().toUtf8()
                  << std::endl;
        return;
    }

#ifndef HAS_SECURITY
    std::cout << "Client library does not support security features." << std::endl;
#endif

    setupClientSecurityInfo(securityInfo, &name, debug);
    setupIdentity();

    for (OpcUa_UInt32 i = 0; i < applicationDescriptions.length(); i++) {
        for (OpcUa_Int32 j = 0; j < applicationDescriptions[i].NoOfDiscoveryUrls; j++) {
            std::cout << "Session " << name << "    (discovery at " << serverURL.toUtf8() << ")"
                      << "\n  Server name: " << UaString(applicationDescriptions[i].ApplicationName.Text).toUtf8()
                      << "\n  Server URI:  " << UaString(applicationDescriptions[i].ApplicationUri).toUtf8()
                      << "\n  Server URL:  " << UaString(applicationDescriptions[i].DiscoveryUrls[j]).toUtf8();
            if (serverURL != applicationDescriptions[i].DiscoveryUrls[j])
                std::cout << "    (using " << serverURL.toUtf8() << ")";
            std::cout << "\n  Requested security mode: " << reqSecurityModeString(reqSecurityMode)
                      << "    policy: " << securityPolicyString(reqSecurityPolicyURI.toUtf8())
                      << "\n  Identity: ";
            if (securityInfo.pUserIdentityToken()->getTokenType() == OpcUa_UserTokenType_UserName)
                std::cout << "Username token '" << securityUserName << "'"
                          << " (credentials from " << securityIdentityFile << ")";
            else if (securityInfo.pUserIdentityToken()->getTokenType() == OpcUa_UserTokenType_Certificate)
                std::cout << "Certificate token '" << securityUserName << "'"
                          << " (credentials from " << securityIdentityFile << ")";
            else
                std::cout << "Anonymous";

            if (std::string(serverURL.toUtf8()).compare(0, 7, "opc.tcp") == 0) {
                UaEndpointDescriptions endpointDescriptions;
                status = discovery.getEndpoints(serviceSettings, serverURL, securityInfo, endpointDescriptions);
                if (status.isBad()) {
                    std::cerr << "Session " << name << ": (showSecurity) UaDiscovery::getEndpoints failed"
                              << " with status" << status.toString().toUtf8()
                              << std::endl;
                    return;
                }

                for (OpcUa_UInt32 k = 0; k < endpointDescriptions.length(); k++) {
                    if (std::string(UaString(endpointDescriptions[k].EndpointUrl).toUtf8()).compare(0, 7, "opc.tcp") == 0) {
                        char dash = '-';
                        std::string marker;
                        if (isConnected()
                            && endpointDescriptions[k].SecurityMode
                                   == securityInfo.messageSecurityMode
                            && UaString(endpointDescriptions[k].SecurityPolicyUri)
                                   == securityInfo.sSecurityPolicy) {
                            dash = '=';
                            marker = " connected =====";
                        }
                        std::cout << "\n  " << std::setfill(dash) << std::setw(5) << dash
                                  << " Endpoint " << k << " " << std::setw(5) << dash
                                  << std::setfill(' ') << " Level: " << std::setw(3)
                                  << +endpointDescriptions[k].SecurityLevel << " "
                                  << std::setfill(dash) << std::setw(50) << dash
                                  << std::setfill(' ')  << marker
                                  << "\n    Security Mode: " << std::setw(14) << std::left
                                  << securityModeString(endpointDescriptions[k].SecurityMode)
                                  << std::right << "    Policy: "
                                  << securityPolicyString(
                                         UaString(endpointDescriptions[k].SecurityPolicyUri).toUtf8())
                                  << "\n    URL: "
                                  << UaString(&endpointDescriptions[k].EndpointUrl).toUtf8();
                        if (UaString(endpointDescriptions[k].EndpointUrl)
                                == UaString(applicationDescriptions[i].DiscoveryUrls[j])
                            && UaString(endpointDescriptions[k].EndpointUrl) != serverURL)
                            std::cout << "    (using " << serverURL.toUtf8() << ")";

                        securityInfo.serverCertificate = endpointDescriptions[k].ServerCertificate;
#ifdef HAS_SECURITY
                        UaPkiCertificate cert = UaPkiCertificate::fromDER(securityInfo.serverCertificate);
                        UaPkiIdentity id = cert.subject();
                        std::cout << "\n    Server Certificate: " << id.commonName.toUtf8()
                                  << " (" << id.organization.toUtf8() << ")"
                                  << " serial " << cert.serialNumber().toUtf8()
                                  << " (thumb " << cert.thumbPrint().toHex(false).toUtf8() << ")"
                                  << (cert.isSelfSigned() ? " self-signed" : "")
                                  << (securityInfo.verifyServerCertificate().isBad() ? " - not" : " -")
                                  << " trusted";
#endif
                    }
                }
                std::cout << std::endl;
            }
        }
    }
}

SessionUaSdk::ConnectResult
SessionUaSdk::setupSecurity ()
{
#ifdef HAS_SECURITY
    if (reqSecurityMode == RequestedSecurityMode::None) {
#endif
        securityInfo.messageSecurityMode = OpcUa_MessageSecurityMode_None;
        securityInfo.sSecurityPolicy = OpcUa_SecurityPolicy_None;
        securityLevel = 0;
        securityInfo.serverCertificate.clear();
        setupIdentity();

        if (debug)
            std::cout << "Session " << name.c_str()
                      << ": (setupSecurity) no security configured "
                      << std::endl;
        return ConnectResult::ok;
#ifdef HAS_SECURITY
    } else {
        setupIdentity();
        if (std::string(serverURL.toUtf8()).compare(0, 7, "opc.tcp") == 0) {
            UaStatus status;
            ServiceSettings serviceSettings;
            UaDiscovery discovery;
            UaEndpointDescriptions endpointDescriptions;
            if (debug)
                std::cout << "Session " << name.c_str()
                          << ": (setupSecurity) reading endpoints from " << serverURL.toUtf8()
                          << std::endl;

            status = discovery.getEndpoints(serviceSettings, serverURL, securityInfo, endpointDescriptions);
            if (status.isBad()) {
                if (debug)
                    std::cout << "Session " << name.c_str()
                              << ": (setupSecurity) UaDiscovery::getEndpoints from " << serverURL.toUtf8()
                              << " failed with status " << status.toString().toUtf8()
                              << std::endl;
                return ConnectResult::cantConnect;
            }

            OpcUa_Byte selectedSecurityLevel = 0;
            OpcUa_UInt32 selectedEndpoint = 0;
            for (OpcUa_UInt32 k = 0; k < endpointDescriptions.length(); k++) {
                if (std::string(UaString(endpointDescriptions[k].EndpointUrl).toUtf8()).compare(0, 7, "opc.tcp") == 0) {
                    if (reqSecurityMode == RequestedSecurityMode::Best ||
                        OpcUaSecurityMode(reqSecurityMode) == endpointDescriptions[k].SecurityMode)
                        if (reqSecurityPolicyURI == OpcUa_SecurityPolicy_None ||
                                reqSecurityPolicyURI == endpointDescriptions[k].SecurityPolicyUri)
                            if (endpointDescriptions[k].SecurityLevel > selectedSecurityLevel
                                && securitySupportedPolicies.count(
                                    UaString(endpointDescriptions[k].SecurityPolicyUri).toUtf8())) {
                                selectedEndpoint = k;
                                selectedSecurityLevel = endpointDescriptions[k].SecurityLevel;
                            }
                }
            }
            if (selectedEndpoint > 0) {
                securityInfo.messageSecurityMode = endpointDescriptions[selectedEndpoint].SecurityMode;
                securityInfo.sSecurityPolicy.attach(&endpointDescriptions[selectedEndpoint].SecurityPolicyUri);
                OpcUa_String_Initialize(&endpointDescriptions[selectedEndpoint].SecurityPolicyUri);
                securityInfo.serverCertificate.attach(&endpointDescriptions[selectedEndpoint].ServerCertificate);
                OpcUa_ByteString_Initialize(&endpointDescriptions[selectedEndpoint].ServerCertificate);
                securityLevel = endpointDescriptions[selectedEndpoint].SecurityLevel;

                // Verify server certificate (and save if rejected and securitySaveRejected is set)
                UaPkiCertificate cert = UaPkiCertificate::fromDER(securityInfo.serverCertificate);
                UaPkiIdentity id = cert.subject();
                if (securityInfo.verifyServerCertificate().isBad()) {
                    if (securitySaveRejected) {
                        UaDir dirHelper("");
                        UaUniString rejectedDir(UaDir::fromNativeSeparators(securitySaveRejectedDir.c_str()));
                        bool save = true;
                        if (!dirHelper.exists(rejectedDir)) {
                            if (!dirHelper.mkpath(rejectedDir)) {
                                std::cout << "Session " << name
                                          << ": (setupSecurity) cannot create directory for "
                                             "rejected certificates ("
                                          << UaString(rejectedDir.toUtf16()).toUtf8() << ")" << std::endl;
                                save = false;
                            }
                        }
                        if (save) {
                            std::string cleanCN(
                                replaceInvalidFilenameChars(id.commonName.toUtf8()).c_str());
                            UaString fileName = UaString("%1/%2 [%3].der")
                                                    .arg(securitySaveRejectedDir.c_str())
                                                    .arg(cleanCN.c_str())
                                                    .arg(cert.thumbPrint().toHex().toUtf8());
                            UaUniString uniFileName(UaDir::fromNativeSeparators(fileName.toUtf16()));
                            if (!dirHelper.exists(uniFileName)) {
                                int status = cert.toDERFile(
                                    UaDir::toNativeSeparators(uniFileName).toLocal8Bit());
                                if (status)
                                    std::cout << "Session " << name
                                              << ": (setupSecurity) ERROR - cannot save rejected "
                                                 "certificate as "
                                              << fileName.toUtf8() << std::endl;
                                else
                                    std::cout << "Session " << name
                                              << ": (setupSecurity) saved rejected certificate as "
                                              << UaString(uniFileName.toUtf16()).toUtf8() << std::endl;
                            }
                        }
                    }
                } else {
                    if (debug)
                        std::cout << "Session " << name.c_str()
                                  << ": (setupSecurity) found matching endpoint, using"
                                  << " mode="
                                  << securityModeString(securityInfo.messageSecurityMode)
                                  << " policy="
                                  << securityPolicyString(securityInfo.sSecurityPolicy.toUtf8())
                                  << " (level " << +securityLevel << ")" << std::endl;
                }
            } else {
                if (debug)
                    std::cout << "Session " << name.c_str()
                              << ": (setupSecurity) found no endpoint that matches"
                              << " the security requirements" << std::endl;
                return ConnectResult::noMatchingEndpoint;
            }
            return ConnectResult::ok;
        } else {
            errlogPrintf("OPC UA session %s: fatal - only URLs of type 'opc.tcp' supported\n",
                         name.c_str());
            return SessionUaSdk::fatal;
        }
    }
    return ConnectResult::ok;
#endif // #ifdef HAS_SECURITY
}

void
SessionUaSdk::setupClientSecurityInfo(ClientSecurityInfo &securityInfo,
                                      const std::string *sessionName,
                                      const int debug)
{
#ifdef HAS_SECURITY
    if (debug) {
        if (sessionName)
            std::cout << "Session " << *sessionName;
        std::cout << ": (setupClientSecurityInfo) setting up PKI provider" << std::endl;
    }
    UaStatus status
        = securityInfo.initializePkiProviderOpenSSL(securityCertificateRevocationListDir.c_str(),
                                                    securityCertificateTrustListDir.c_str(),
                                                    securityIssuersCertificatesDir.c_str(),
                                                    securityIssuersRevocationListDir.c_str());
    if (status.isBad())
        errlogPrintf("%s%s: setting up PKI context failed with status %s\n",
                     sessionName ? "OPC UA Session " : "OPC UA",
                     sessionName ? sessionName->c_str() : "",
                     status.toString().toUtf8());

    if (securityClientCertificateFile.length() && securityClientPrivateKeyFile.length()) {
        if (debug) {
            if (sessionName)
                std::cout << "Session " << *sessionName;
            std::cout << ": (setupClientSecurityInfo) loading client certificate "
                      << securityClientCertificateFile << std::endl;
        }

        UaPkiCertificate cert;
        if (isPEM(securityClientCertificateFile)) {
            cert = UaPkiCertificate::fromPEMFile(UaString(securityClientCertificateFile.c_str()));
        } else {
            cert = UaPkiCertificate::fromDERFile(UaString(securityClientCertificateFile.c_str()));
        }

        if (!cert.isValid()) {
            errlogPrintf("%s%s: configured client certificate is not valid (expired?)\n",
                         sessionName ? "OPC UA Session " : "OPC UA",
                         sessionName ? sessionName->c_str() : "");
            return;
        }

        if (debug) {
            if (sessionName)
                std::cout << "Session " << *sessionName;
            std::cout << ": (setupClientSecurityInfo) loading client private key "
                      << securityClientPrivateKeyFile << std::endl;
        }

        // atm hard-coded: no password for client certificate key
        auto key = UaPkiRsaKeyPair::fromPEMFile(securityClientPrivateKeyFile.c_str(), "");

        if (!UaPkiRsaKeyPair::checkKeyPair(cert.publicKey(), key.privateKey())) {
            errlogPrintf("%s%s: client certificate and client key don't match\n",
                         sessionName ? "OPC UA Session " : "OPC UA",
                         sessionName ? sessionName->c_str() : "");
            return;
        }

// API change in UA SDK 1.6
#if (PROD_MAJOR == 1 && PROD_MINOR < 6)
        securityInfo.clientPrivateKey = key.toDER();
#else
        securityInfo.setClientPrivateKeyDer(key.toDER());
#endif
        securityInfo.clientCertificate = cert.toDER();
    } else {
        if (debug) {
            if (sessionName)
                std::cout << "Session " << *sessionName;
            std::cout << ": (setupClientSecurityInfo) no client certificate configured"
                      << std::endl;
        }
    }
#endif // #ifdef HAS_SECURITY
}

void
SessionUaSdk::show (const int level) const
{
    std::cout << "session="      << name
              << " url="         << serverURL.toUtf8()
              << " status="      << serverStatusString(serverConnectionStatus)
              << " sec-mode="    << securityModeString(securityInfo.messageSecurityMode)
              << "(" << reqSecurityModeString(reqSecurityMode) << ")"
              << " sec-policy="  << securityPolicyString(securityInfo.sSecurityPolicy.toUtf8())
              << "(" << securityPolicyString(reqSecurityPolicyURI.toUtf8()) << ")"
              << " debug="       << debug
              << " batch=";
    if (isConnected())
        std::cout << puasession->maxOperationsPerServiceCall();
    else
        std::cout << "?";
    std::cout << "(" << connectInfo.nMaxOperationsPerServiceCall << ")"
              << " autoconnect=" << (autoConnect ? "y" : "n")
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

inline void
SessionUaSdk::markConnectionLoss()
{
    reader.clear();
    writer.clear();
    for (auto it : items) {
        it->setState(ConnectionStatus::down);
        it->setIncomingEvent(ProcessReason::connectionLoss);
    }
}

void
SessionUaSdk::setupIdentity()
{
    securityInfo.setAnonymousUserIdentity();

#ifdef HAS_SECURITY
    if (securityIdentityFile.length()) {
        std::ifstream inFile;
        std::string line;
        std::string user;
        std::string certfile;
        std::string keyfile;
        std::string pass;

        inFile.open(securityIdentityFile);
        if (inFile.fail()) {
            errlogPrintf("OPC UA session %s: cannot open credentials file %s\n",
                         name.c_str(),
                         securityIdentityFile.c_str());
        }

        while (std::getline(inFile, line)) {
            size_t hash = line.find_first_of('#');
            size_t equ = line.find_first_of('=');
            if (hash < equ) continue;
            if (equ != std::string::npos) {
                if (line.substr(0, equ) == "user")
                    user = line.substr(equ + 1, std::string::npos);
                else if (line.substr(0, equ) == "pass")
                    pass = line.substr(equ + 1, std::string::npos);
                else if (line.substr(0, equ) == "cert")
                    certfile = line.substr(equ + 1, std::string::npos);
                else if (line.substr(0, equ) == "key")
                    keyfile = line.substr(equ + 1, std::string::npos);
            }
        }

        if (user.length() && pass.length()) {
            if (debug)
                std::cout << "Session " << name.c_str()
                          << ": (setupIdentity) setting Username token (" << user
                          << "/*****)" << std::endl;
            securityUserName = user;
            securityInfo.setUserPasswordUserIdentity(user.c_str(), pass.c_str());

        } else if (certfile.length() && keyfile.length()) {
            if (debug)
                std::cout << "Session " << name << ": (setupIdentity) loading identity certificate "
                          << certfile << std::endl;

            UaPkiCertificate cert;
            if (isPEM(certfile)) {
                cert = UaPkiCertificate::fromPEMFile(certfile.c_str());
            } else {
                cert = UaPkiCertificate::fromDERFile(certfile.c_str());
            }
            if (!cert.isValid())
                errlogPrintf("OPC UA session %s: identity certificate is not valid (expired?)\n",
                             name.c_str());
            UaPkiRsaKeyPair key = UaPkiRsaKeyPair::fromPEMFile(keyfile.c_str(), pass.c_str());
            if (debug)
                std::cout << "Session " << name.c_str()
                          << ": (setupIdentity) setting Certificate token (CN="
                          << cert.commonName().toUtf8() << "; thumb "
                          << cert.thumbPrint().toHex(false).toUtf8() << ")" << std::endl;
            securityUserName = cert.commonName().toUtf8();
            securityInfo.setCertificateUserIdentity(cert.toByteStringDER(), key.toDER());

        } else {
            errlogPrintf(
                "OPC UA session %s: credentials file %s does not contain settings for "
                "Username token (user + pass) or Certificate token (cert + key [+ pass])\n",
                name.c_str(),
                securityIdentityFile.c_str());
        }
    } else
#endif // #ifdef HAS_SECURITY
    {
        if (debug)
            std::cout << "Session " << name.c_str() << ": (setupIdentity) setting Anonymous token"
                      << std::endl;
    }
}

// UaSessionCallback interface

void SessionUaSdk::connectionStatusChanged (
    OpcUa_UInt32             clientConnectionId,
    UaClient::ServerStatus   serverStatus)
{
    OpcUa_ReferenceParameter(clientConnectionId);
    // Don't print Disconnected <-> ConnectionErrorApiReconnect unless in debug mode
    if (debug)
        std::cerr << "Session " << name.c_str() << ": connection status changed from "
                  << serverStatusString(serverConnectionStatus) << " to "
                  << serverStatusString(serverStatus) << std::endl;

    switch (serverStatus) {

        // "The monitoring of the connection to the server detected an error
        // and is trying to reconnect to the server."
    case UaClient::ConnectionErrorApiReconnect:
        // "The server sent a shut-down event and the client API tries a reconnect."
    case UaClient::ServerShutdown:
        if (serverConnectionStatus == UaClient::Connected
            || serverConnectionStatus == UaClient::ConnectionWarningWatchdogTimeout)
            markConnectionLoss();
        if (serverStatus == UaClient::ServerShutdown)
            registeredItemsNo = 0;
        if (serverConnectionStatus != UaClient::Disconnected)
            errlogPrintf("OPC UA session %s: disconnected\n", name.c_str());
        if (autoConnect)
            autoConnector.start();
        break;

        // "The connection to the server is deactivated by the user of the client API."
    case UaClient::Disconnected:
        if (serverConnectionStatus == UaClient::Connected) {
            errlogPrintf("OPC UA session %s: disconnected\n", name.c_str());
            markConnectionLoss();
            registeredItemsNo = 0;
        }

        break;

        // "The monitoring of the connection to the server indicated
        // a potential connection problem."
    case UaClient::ConnectionWarningWatchdogTimeout:
        break;

        // "The connection to the server is established and is working in normal mode."
    case UaClient::Connected:
        if (serverConnectionStatus == UaClient::Disconnected
            || serverConnectionStatus == UaClient::ConnectionErrorApiReconnect
            || serverConnectionStatus == UaClient::NewSessionCreated) {
            std::string token;
            auto type = securityInfo.pUserIdentityToken()->getTokenType();
            if (type == OpcUa_UserTokenType_UserName)
                token = " (username token)";
            else if (type == OpcUa_UserTokenType_Certificate)
                token = " (certificate token)";
            errlogPrintf(
                "OPC UA session %s: connected as '%s'%s (sec-mode: %s; sec-policy: %s)\n",
                name.c_str(),
                (securityUserName.length() ? securityUserName.c_str() : "Anonymous"),
                token.c_str(),
                securityModeString(securityInfo.messageSecurityMode),
                securityPolicyString(securityInfo.sSecurityPolicy.toUtf8()).c_str());
            if (securityInfo.messageSecurityMode == OpcUa_MessageSecurityMode_None) {
                errlogPrintf("OPC UA session %s: WARNING - this session uses *** NO SECURITY ***\n",
                             name.c_str());
            }
        }
        if (serverConnectionStatus == UaClient::Disconnected) {
            updateNamespaceMap(puasession->getNamespaceTable());
            rebuildNodeIds();
            registerNodes();
            createAllSubscriptions();
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
            epicsThreadSleep(0.01); // Don't know how to wait for initial read to complete
            addAllMonitoredItems();
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

const EnumChoices*
SessionUaSdk::getEnumChoices(const UaEnumDefinition& enumDefinition) const
{
    if (!enumDefinition.childrenCount())
        return nullptr;
    auto enumChoices = new EnumChoices;
    for (int i = 0; i < enumDefinition.childrenCount(); i++) {
        const UaEnumValue& choice = enumDefinition.child(i);
        enumChoices->emplace(choice.value(), choice.name().toUtf8());
    }
    return enumChoices;
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
        if ((*it->second).size() * no_of_properties_read != values.length())
            errlogPrintf("OPC UA session %s: (readComplete) received a callback "
                         "with %u values for a request containing %lu items\n",
                         name.c_str(), values.length(), (*it->second).size());
        OpcUa_UInt32 i = 0;
        for (auto item : (*it->second)) {
            if (i >= values.length()) {
                item->setIncomingEvent(ProcessReason::readFailure);
            } else {
                UaNodeId typeId;
                if (OpcUa_IsGood(values[i].StatusCode)) {
                    UaVariant(values[i].Value).toNodeId(typeId);
                }
                i++;
                if (debug >= 5) {
                    std::cout << "** Session " << name.c_str()
                              << ": (readComplete) getting data for item "
                              << item->getNodeId().toXmlString().toUtf8() << std::endl;
                }
                ProcessReason reason = ProcessReason::readComplete;
                if (OpcUa_IsNotGood(values[i].StatusCode))
                    reason = ProcessReason::readFailure;
                item->setIncomingData(values[i], reason, &typeId);
                i++;
            }
        }
        outstandingOps.erase(it);
    } else {
        if (debug)
            std::cout << "Session " << name.c_str()
                      << ": (readComplete) for read service"
                      << " (transaction id " << transactionId
                      << ") failed with status " << result.toString().toUtf8() << std::endl;
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
                      << ") failed with status " << result.toString().toUtf8() << std::endl;
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
    case initHookAfterIocRunning:
    {
        errlogPrintf("OPC UA: Autoconnecting sessions\n");
        for (auto &it : sessions) {
            it.second->markConnectionLoss();
            if (it.second->autoConnect)
                it.second->connect(false);
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
        SessionUaSdk *session = it.second;
        // See #130 and reverted commit ab7184ef
        // Make sure low-level session is valid before running disconnect()
        if (session->puasession && session->isConnected())
            session->disconnect();
    }
}

} // namespace DevOpcua
