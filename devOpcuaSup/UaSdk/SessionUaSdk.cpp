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
#include <string>
#include <map>
#include <algorithm>
#include <utility>
#include <vector>
#include <limits>

#include <uaclientsdk.h>
#include <uasession.h>
#include <uadiscovery.h>
#include <uapkicertificate.h>

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
#include "DataElementUaSdk.h"
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
securityPolicyString (const UaString &policy)
{
    auto s = std::string(policy.toUtf8());
    size_t found = s.find_last_of('#');
    if (found == std::string::npos)
        return "Invalid";
    else
        return policy.toUtf8() + found + 1;
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
                           const std::string &serverUrl,
                           bool autoConnect,
                           int debug)
    : Session(debug, autoConnect)
    , name(name)
    , serverURL(serverUrl.c_str())
    , registeredItemsNo(0)
    , puasession(new UaSession())
    , reqSecurityMode(OpcUa_MessageSecurityMode_None)
    , reqSecurityPolicyURI("http://opcfoundation.org/UA/SecurityPolicy#None")
    , reqSecurityLevel(0)
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

    if (name == "sec-mode") {
        if (value == "None" || value == "none") {
            reqSecurityMode = OpcUa_MessageSecurityMode_None;
        } else if (value == "SignAndEncrypt") {
            reqSecurityMode = OpcUa_MessageSecurityMode_SignAndEncrypt;
        } else if (value == "Sign") {
            reqSecurityMode = OpcUa_MessageSecurityMode_Sign;
        } else {
            errlogPrintf("invalid security-mode (valid: None Sign SignAndEncrypt)\n");
            reqSecurityMode = OpcUa_MessageSecurityMode_Invalid;
        }
    } else if (name == "sec-policy") {
        if (value == "None" || value == "none") {
            reqSecurityPolicyURI = OpcUa_SecurityPolicy_None;
#if OPCUA_SUPPORT_SECURITYPOLICY_BASIC128RSA15
        } else if (value == "Basic128Rsa15") {
            reqSecurityPolicyURI = OpcUa_SecurityPolicy_Basic128Rsa15;
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_BASIC256
        } else if (value == "Basic256") {
            reqSecurityPolicyURI = OpcUa_SecurityPolicy_Basic256;
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_BASIC256SHA256
        } else if (value == "Basic256Sha256") {
            reqSecurityPolicyURI = OpcUa_SecurityPolicy_Basic256Sha256;
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_AES128SHA256RSAOAEP
        } else if (value == "Aes128Sha256RsaOaep") {
            reqSecurityPolicyURI = OpcUa_SecurityPolicy_Aes128Sha256RsaOaep;
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_AES256SHA256RSAPSS
        } else if (value == "Aes256Sha256RsaPss") {
            reqSecurityPolicyURI = OpcUa_SecurityPolicy_Aes256Sha256RsaPss;
#endif
        } else {
            errlogPrintf("invalid security-policy (valid: None"
#if OPCUA_SUPPORT_SECURITYPOLICY_BASIC128RSA15
                         " Basic128Rsa15"
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_BASIC256
                         " Basic256"
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_BASIC256SHA256
                         " Basic256Sha256"
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_AES128SHA256RSAOAEP
                         " Aes128Sha256RsaOaep"
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_AES256SHA256RSAPSS
                         " Aes256Sha256RsaPss"
#endif
                         ")\n");
        }
    } else if (name == "sec-level-min") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        reqSecurityLevel = static_cast<unsigned char>(ul);
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
SessionUaSdk::connect()
{
    if (!puasession) {
        std::cerr << "Session " << name.c_str() << ": invalid session, cannot connect" << std::endl;
        return -1;
    }

    if (isConnected()) {
        if (debug)
            std::cerr << "Session " << name.c_str() << ": already connected ("
                      << serverStatusString(serverConnectionStatus) << ")" << std::endl;
        return 0;
    }

    disconnect(); // Do a proper disconnection before attempting to reconnect

    ConnectResult secResult = setupSecurity();
    if (secResult) {
        if (!autoConnect || debug)
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
        if (securityInfo.messageSecurityMode != OpcUa_MessageSecurityMode_None)
            errlogPrintf("OPC UA session %s: connect service succeeded with security level %u "
                         "(mode=%s; policy=%s)\n",
                         name.c_str(),
                         securityLevel,
                         securityModeString(securityInfo.messageSecurityMode),
                         securityPolicyString(securityInfo.sSecurityPolicy));
        else
            errlogPrintf("OPC UA session %s: connect service succeeded with no security\n",
                         name.c_str());
    } else {
        if (!autoConnect || debug)
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
            //	    item.setIncomingEvent(ProcessReason::readFailure);

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
            //	    item.setIncomingEvent(ProcessReason::writeFailure);

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

    for (OpcUa_UInt32 i = 0; i < applicationDescriptions.length(); i++) {
        for (OpcUa_Int32 j = 0; j < applicationDescriptions[i].NoOfDiscoveryUrls; j++) {
            std::cout << "Session " << name << "    (discovery at " << serverURL.toUtf8() << ")"
                      << "\n  Server Name: " << UaString(applicationDescriptions[i].ApplicationName.Text).toUtf8()
                      << "\n  Server Uri:  " << UaString(applicationDescriptions[i].ApplicationUri).toUtf8()
                      << "\n  Server Url:  " << UaString(applicationDescriptions[i].DiscoveryUrls[j]).toUtf8();
            if (serverURL != applicationDescriptions[i].DiscoveryUrls[j])
                std::cout << "    (using " << serverURL.toUtf8() << ")";
            std::cout << "\n  Requested Mode: " << securityModeString(reqSecurityMode)
                      << "    Policy: " << securityPolicyString(reqSecurityPolicyURI)
                      << "    Level: " << +reqSecurityLevel;
            if (std::string(serverURL.toUtf8()).compare(0, 7, "opc.tcp") == 0) {
                UaEndpointDescriptions endpointDescriptions;
                status = discovery.getEndpoints(serviceSettings, serverURL, securityInfo, endpointDescriptions);
                if (status.isBad()) {
                    std::cerr << "Session " << name << ": (showSecurity) UaDiscovery::getEndpoints failed"
                              << " with status" << status.toString()
                              << std::endl;
                    return;
                }

                for (OpcUa_UInt32 k = 0; k < endpointDescriptions.length(); k++) {
                    if (std::string(UaString(endpointDescriptions[k].EndpointUrl).toUtf8()).compare(0, 7, "opc.tcp") == 0) {
                        char dash = '-';
                        if (isConnected()
                            && endpointDescriptions[k].SecurityMode
                                   == securityInfo.messageSecurityMode
                            && UaString(endpointDescriptions[k].SecurityPolicyUri)
                                   == securityInfo.sSecurityPolicy)
                            dash = '=';
                        std::cout << "\n  " << std::setfill(dash) << std::setw(5) << dash
                                  << std::setfill(' ') << " Level: " << std::setw(3)
                                  << +endpointDescriptions[k].SecurityLevel << " "
                                  << std::setfill(dash) << std::setw(45) << dash
                                  << std::setfill(' ') << " Endpoint " << k
                                  << "\n    Security Mode: "
                                  << securityModeString(endpointDescriptions[k].SecurityMode)
                                  << "    Policy: "
                                  << securityPolicyString(
                                         UaString(endpointDescriptions[k].SecurityPolicyUri))
                                  << "\n    URL: "
                                  << UaString(&endpointDescriptions[k].EndpointUrl).toUtf8();
                        if (UaString(endpointDescriptions[k].EndpointUrl) == UaString(applicationDescriptions[i].DiscoveryUrls[j]))
                            std::cout << "    (using " << serverURL.toUtf8() << ")";

                        securityInfo.serverCertificate = endpointDescriptions[k].ServerCertificate;

                        UaPkiCertificate cert = UaPkiCertificate::fromDER(securityInfo.serverCertificate);
                        UaPkiIdentity id = cert.subject();
                        std::cout << "\n    Server Certificate: " << id.commonName.toUtf8()
                                  << " (" << id.organization.toUtf8() << ")"
                                  << " serial " << cert.serialNumber().toUtf8()
                                  << " (thumb " << cert.thumbPrint().toHex(false).toUtf8() << ")"
                                  << (cert.isSelfSigned() ? " self-signed" : "")
                                  << (securityInfo.verifyServerCertificate().isBad() ? " - not" : " -")
                                  << " trusted";
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
    UaStatus status;
    ServiceSettings serviceSettings;
    UaDiscovery discovery;

    if (reqSecurityMode == OpcUa_MessageSecurityMode_None
        && reqSecurityPolicyURI == OpcUa_SecurityPolicy_None && reqSecurityLevel == 0) {
        securityInfo.messageSecurityMode = OpcUa_MessageSecurityMode_None;
        securityInfo.sSecurityPolicy = OpcUa_SecurityPolicy_None;
        securityInfo.serverCertificate.clear();

        if (debug)
            std::cout << "Session " << name.c_str()
                      << ": (setupSecurity) no security configured "
                      << std::endl;
        return ConnectResult::ok;

    } else {

        if (std::string(serverURL.toUtf8()).compare(0, 7, "opc.tcp") == 0) {
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

            OpcUa_Byte selectedSecurityLevel = reqSecurityLevel;
            OpcUa_UInt32 selectedEndpoint = 0;
            for (OpcUa_UInt32 k = 0; k < endpointDescriptions.length(); k++) {
                if (std::string(UaString(endpointDescriptions[k].EndpointUrl).toUtf8()).compare(0, 7, "opc.tcp") == 0) {
                    if (reqSecurityMode == OpcUa_MessageSecurityMode_None ||
                            reqSecurityMode == endpointDescriptions[k].SecurityMode)
                        if (reqSecurityPolicyURI == OpcUa_SecurityPolicy_None ||
                                reqSecurityPolicyURI == endpointDescriptions[k].SecurityPolicyUri)
                            if (endpointDescriptions[k].SecurityLevel >= selectedSecurityLevel) {
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
                if (debug)
                    std::cout << "Session " << name.c_str()
                              << ": (setupSecurity) found matching endpoint, using"
                              << " mode=" << securityModeString(securityInfo.messageSecurityMode)
                              << " policy=" << securityPolicyString(securityInfo.sSecurityPolicy)
                              << " (level " << +endpointDescriptions[selectedEndpoint].SecurityLevel << ")"
                              << std::endl;
            } else {
                errlogPrintf("OPC UA session %s: (setupSecurity) found no endpoint that matches "
                             "the security requirements",
                             name.c_str());
                return ConnectResult::noMatchingEndpoint;
            }
            return ConnectResult::ok;
        } else {
            errlogPrintf("OPC UA session %s: fatal - only URLs of type 'opc.tcp' supported\n",
                         name.c_str());
            return SessionUaSdk::fatal;
        }
    }
}

void SessionUaSdk::initClientSecurity()
{
    UaStatus status;

    if (debug)
        std::cout << "Session " << name.c_str()
                  << ": (initClientSecurity) setting up PKI provider"
                  << std::endl;
    status = securityInfo.initializePkiProviderOpenSSL(
                securityCertificateRevocationListDir.c_str(),
                securityCertificateTrustListDir.c_str(),
                securityIssuersCertificatesDir.c_str(),
                securityIssuersRevocationListDir.c_str());
    if (status.isBad())
        errlogPrintf("OPC UA session %s: setting up PKI context failed with status %s\n",
                     name.c_str(), status.toString().toUtf8());

    if (debug)
        std::cout << "Session " << name.c_str()
                  << ": (initClientSecurity) loading client certificate " << securityClientCertificateFile.c_str()
                  << std::endl;
    status = securityInfo.loadClientCertificateOpenSSL(
                securityClientCertificateFile.c_str(),
                securityClientPrivateKeyFile.c_str());
    if (status.isBad())
        errlogPrintf("OPC UA session %s: loading client certificate failed with status %s\n",
                     name.c_str(), status.toString().toUtf8());
}

void
SessionUaSdk::show (const int level) const
{
    std::cout << "session="      << name
              << " url="         << serverURL.toUtf8()
              << " status="      << serverStatusString(serverConnectionStatus)
              << " sec-mode="    << securityModeString(securityInfo.messageSecurityMode)
              << "(" << securityModeString(reqSecurityMode) << ")"
              << " sec-policy="  << securityPolicyString(securityInfo.sSecurityPolicy)
              << "(" << securityPolicyString(reqSecurityPolicyURI) << ")"
              << " sec-level="   << reqSecurityLevel
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
    registeredItemsNo = 0;
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
        markConnectionLoss();
        if (autoConnect)
            autoConnector.start();
        break;
        // "The connection to the server is deactivated by the user of the client API."
    case UaClient::Disconnected:
        if (serverConnectionStatus == UaClient::Connected)
            markConnectionLoss();
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
            it.second->markConnectionLoss();
            it.second->initClientSecurity();
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
