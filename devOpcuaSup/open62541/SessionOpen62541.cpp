/*************************************************************************\
* Copyright (c) 2018-2023 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 *  and the custom type prototype by Carsten Winkler <carsten.winkler@helmholtz-berlin.de>
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <algorithm>
#include <utility>
#include <vector>
#include <limits>

#include <epicsExit.h>
#include <epicsThread.h>
#include <epicsAtomic.h>
#include <osdSock.h>
#include <initHooks.h>
#include <errlog.h>

#include <open62541/client.h>
#include <open62541/client_highlevel.h>
#include <open62541/client_highlevel_async.h>
#include <open62541/plugin/pki_default.h>

/* loadFile helper from open62541 examples */
#include <cstdio>

#include <open62541/types.h>
#include <open62541/types_generated_handling.h>

/* loadFile parses the certificate file.
 *
 * @param  path               specifies the file name given in argv[]
 * @return Returns the file content after parsing */
static UA_INLINE UA_ByteString
loadFile(const char *const path) {
    UA_ByteString fileContents = UA_STRING_NULL;

    /* Open the file */
    FILE *fp = fopen(path, "rb");
    if(!fp) {
        errno = 0; /* We read errno also from the tcp layer... */
        return fileContents;
    }

    /* Get the file length, allocate the data and read */
    fseek(fp, 0, SEEK_END);
    fileContents.length = (size_t)ftell(fp);
    fileContents.data = (UA_Byte *)UA_malloc(fileContents.length * sizeof(UA_Byte));
    if(fileContents.data) {
        fseek(fp, 0, SEEK_SET);
        size_t read = fread(fileContents.data, sizeof(UA_Byte), fileContents.length, fp);
        if(read != fileContents.length)
            UA_ByteString_clear(&fileContents);
    } else {
        fileContents.length = 0;
    }
    fclose(fp);

    return fileContents;
}
/* end of included loadFile helper code */

#ifndef UA_BUILTIN_TYPES_COUNT
// Newer open62541 since version 1.3 uses type pointer
#define UA_DATATYPES_USE_POINTER
// Older open62541 uses type index
#endif

#ifdef HAS_SECURITY
extern const UA_String username_policy;
extern const UA_String certificate_policy;
#endif

#ifdef HAS_XMLPARSER
#include <libxml/tree.h>
#endif

#define epicsExportSharedSymbols
#include "Session.h"
#include "RecordConnector.h"
#include "linkParser.h"
#include "RequestQueueBatcher.h"
#include "SessionOpen62541.h"
#include "SubscriptionOpen62541.h"
#include "DataElementOpen62541.h"
#include "ItemOpen62541.h"

namespace DevOpcua {

// print some UA types

std::ostream&
operator << (std::ostream& os, const UA_NodeId& ua_nodeId)
{
    UA_String s = UA_STRING_NULL;
    UA_NodeId_print(&ua_nodeId, &s);
    os << s;
    UA_String_clear(&s);
    return os;
}

std::ostream&
operator << (std::ostream& os, const UA_ExtensionObject &ua_extensionObject)
{
    UA_String s = UA_STRING_NULL;
    UA_print(&ua_extensionObject, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT], &s);
    os << "EO:{" << s << '}';
    UA_String_clear(&s);
    return os;
}

std::ostream&
operator << (std::ostream& os, const UA_Variant &ua_variant)
{
    if (ua_variant.data == nullptr) return os << "NO_DATA";
    if (ua_variant.type == nullptr) return os << "NO_TYPE";
    UA_String s = UA_STRING_NULL;
    if (UA_Variant_isScalar(&ua_variant)) {
        UA_print(ua_variant.data, ua_variant.type, &s);
        os << s << " (" << ua_variant.type->typeName << ')';
    } else {
        os << s << '{';
        char* data = static_cast<char*>(ua_variant.data);
        for (size_t i = 0; i < ua_variant.arrayLength; i++) {
            UA_print(data, ua_variant.type, &s);
            data += ua_variant.type->memSize;
            if (i) os << ", ";
            os << s;
            UA_String_clear(&s);
        }
        os << s << "} (" << ua_variant.type->typeName
           << '[' << ua_variant.arrayLength << "])";
    }
    UA_String_clear(&s);
    return os;
}

inline std::ostream&
operator << (std::ostream& os, UA_SecureChannelState channelState)
{
    switch (channelState) {
        case UA_SECURECHANNELSTATE_FRESH:               return os << "Fresh";
#if UA_OPEN62541_VER_MAJOR*100+UA_OPEN62541_VER_MINOR >= 104
        case UA_SECURECHANNELSTATE_REVERSE_LISTENING:   return os << "ReverseListening";
        case UA_SECURECHANNELSTATE_CONNECTING:          return os << "Connecting";
        case UA_SECURECHANNELSTATE_CONNECTED:           return os << "Connected";
        case UA_SECURECHANNELSTATE_REVERSE_CONNECTED:   return os << "ReverseConnected";
        case UA_SECURECHANNELSTATE_RHE_SENT:            return os << "RheSent";
#endif
        case UA_SECURECHANNELSTATE_HEL_SENT:            return os << "HelSent";
        case UA_SECURECHANNELSTATE_HEL_RECEIVED:        return os << "HelReceived";
        case UA_SECURECHANNELSTATE_ACK_SENT:            return os << "AckSent";
        case UA_SECURECHANNELSTATE_ACK_RECEIVED:        return os << "AckReceived";
        case UA_SECURECHANNELSTATE_OPN_SENT:            return os << "OPNSent";
        case UA_SECURECHANNELSTATE_OPEN:                return os << "Open";
        case UA_SECURECHANNELSTATE_CLOSING:             return os << "Closing";
        case UA_SECURECHANNELSTATE_CLOSED:              return os << "Closed";
        default: return os << "<unknown " << static_cast<unsigned int>(channelState) << ">";
    }
}

inline std::ostream&
operator << (std::ostream& os, UA_SessionState sessionState)
{
    switch (sessionState) {
        case UA_SESSIONSTATE_CLOSED:             return os << "Closed";
        case UA_SESSIONSTATE_CREATE_REQUESTED:   return os << "CreateRequested";
        case UA_SESSIONSTATE_CREATED:            return os << "Created";
        case UA_SESSIONSTATE_ACTIVATE_REQUESTED: return os << "ActivateRequested";
        case UA_SESSIONSTATE_ACTIVATED:          return os << "Activated";
        case UA_SESSIONSTATE_CLOSING:            return os << "Closing";
        default: return os << "<unknown " << static_cast<unsigned int>(sessionState) << ">";
    }
}

inline std::ostream&
operator << (std::ostream& os, UA_MessageSecurityMode securityMode)
{
    switch (securityMode) {
        case UA_MESSAGESECURITYMODE_INVALID:         return os << "Invalid";
        case UA_MESSAGESECURITYMODE_NONE:            return os << "None";
        case UA_MESSAGESECURITYMODE_SIGN:            return os << "Sign";
        case UA_MESSAGESECURITYMODE_SIGNANDENCRYPT:  return os << "SignAndEncrypt";
        default: return os << "<unknown " << static_cast<unsigned int>(securityMode) << ">";
    }
}

inline std::ostream&
operator << (std::ostream& os, RequestedSecurityMode mode)
{
    switch (mode) {
        case RequestedSecurityMode::Best:            return os << "best";
        case RequestedSecurityMode::None:            return os << "None";
        case RequestedSecurityMode::Sign:            return os << "Sign";
        case RequestedSecurityMode::SignAndEncrypt:  return os << "SignAndEncrypt";
        default: return os << "<unknown " << static_cast<unsigned int>(mode) << ">";
    }
}

inline UA_MessageSecurityMode
OpcUaSecurityMode(const RequestedSecurityMode mode)
{
    switch (mode) {
        case RequestedSecurityMode::Best:            return UA_MESSAGESECURITYMODE_INVALID;
        case RequestedSecurityMode::None:            return UA_MESSAGESECURITYMODE_NONE;
        case RequestedSecurityMode::Sign:            return UA_MESSAGESECURITYMODE_SIGN;
        case RequestedSecurityMode::SignAndEncrypt:  return UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
        default:                                     return UA_MESSAGESECURITYMODE_INVALID;
    }
}

inline bool
operator == (const std::string& str, const UA_String& ua_string)
{
    return str.compare(0, str.size(), reinterpret_cast<const char*>(ua_string.data), ua_string.length) == 0;
}

inline bool
operator != (const std::string& str, const UA_String& ua_string)
{
    return !(str == ua_string);
}

inline std::string&
operator += (std::string& str, const UA_String& ua_string)
{
    return str.append(reinterpret_cast<const char*>(ua_string.data), ua_string.length);
}

static epicsThreadOnceId session_open62541_ihooks_once = EPICS_THREAD_ONCE_INIT;
static epicsThreadOnceId session_open62541_atexit_once = EPICS_THREAD_ONCE_INIT;

Registry<SessionOpen62541> SessionOpen62541::sessions;

// Cargo structure and batcher for write requests
struct WriteRequest {
    ItemOpen62541 *item;
    UA_WriteValue wvalue;
};

// Cargo structure and batcher for read requests
struct ReadRequest {
    ItemOpen62541 *item;
};

static
void session_open62541_ihooks_register (void*)
{
    initHookRegister(SessionOpen62541::initHook);
}

static
void session_open62541_atexit_register (void *)
{
    epicsAtExit(SessionOpen62541::atExit, nullptr);
}

inline const char *
SessionOpen62541::connectResultString (const ConnectResult result)
{
    switch (result) {
    case ConnectResult::fatal:               return "fatal";
    case ConnectResult::ok:                  return "ok";
    case ConnectResult::cantConnect:         return "cantConnect";
    case ConnectResult::noMatchingEndpoint:  return "noMatchingEndpoint";
    }
    return "";
}

SessionOpen62541::SessionOpen62541 (const std::string &name,
                                    const std::string &serverUrl)
    : Session(name)
    , serverURL(serverUrl)
    , registeredItemsNo(0)
    , reqSecurityMode(RequestedSecurityMode::Best)
    , reqSecurityPolicyUri("http://opcfoundation.org/UA/SecurityPolicy#None")
    , transactionId(0)
    , writer("OPCwr-" + name, *this)
    , writeNodesMax(0)
    , writeTimeoutMin(0)
    , writeTimeoutMax(0)
    , reader("OPCrd-" + name, *this)
    , readNodesMax(0)
    , readTimeoutMin(0)
    , readTimeoutMax(0)
    , client(nullptr)
    , channelState(UA_SECURECHANNELSTATE_CLOSED)
    , sessionState(UA_SESSIONSTATE_CLOSED)
    , connectStatus(UA_STATUSCODE_BADINVALIDSTATE)
    , workerThread(nullptr)
{
    sessions.insert({name, this});
    epicsThreadOnce(&session_open62541_ihooks_once, &session_open62541_ihooks_register, nullptr);
    securityUserName = "Anonymous";
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

void
SessionOpen62541::setOption (const std::string &name, const std::string &value)
{
    bool updateReadBatcher = false;
    bool updateWriteBatcher = false;

    if (debug || name == "debug")
        std::cerr << "Session " << this->name
                  << ": setting option " << name
                  << " to " << value
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
            errlogPrintf("invalid security-mode (valid: best None Sign SignAndEncrypt)\n");
        }
    } else if (name == "sec-policy") {
        bool found = false;
        for (const auto &p : securitySupportedPolicies) {
            if (value == p.second) {
                found = true;
                reqSecurityPolicyUri = p.first;
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
        readNodesMax = ul;
        writeNodesMax = ul;
        updateReadBatcher = true;
        updateWriteBatcher = true;
    } else if (name == "nodes-max") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        readNodesMax = ul;
        writeNodesMax = ul;
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
    if (MaxNodesPerRead > 0 && readNodesMax > 0) {
        max = std::min<unsigned int>(MaxNodesPerRead, readNodesMax);
    } else {
        max = MaxNodesPerRead + readNodesMax;
    }
    if (updateReadBatcher) reader.setParams(max, readTimeoutMin, readTimeoutMax);

    if (MaxNodesPerWrite > 0 && writeNodesMax > 0) {
        max = std::min<unsigned int>(MaxNodesPerWrite, writeNodesMax);
    } else {
        max = MaxNodesPerWrite + writeNodesMax;
    }
    if (updateWriteBatcher) writer.setParams(max, writeTimeoutMin, writeTimeoutMax);
}

long
SessionOpen62541::connect (bool manual)
{
    if (isConnected()) {
        if (debug || manual)
            std::cerr << "Session " << name
                    << " already connected ("
                    << sessionState << ')'
                    << std::endl;
        return 0;
    }

    disconnect(); // Do a proper disconnection before attempting to reconnect

    setupClientSecurityInfo(securityInfo, &name, debug);

    if (!client) {
        client = UA_Client_new();
        if (!client) {
            std::cerr << "Session " << name
                    << ": cannot create new client (out of memory?)"
                    << std::endl;
            return -1;
        }
    }
    UA_ClientConfig *config = UA_Client_getConfig(client);
#ifdef HAS_SECURITY
    // We need the client certificate before UA_ClientConfig_setDefaultEncryption
    UA_ClientConfig_setDefaultEncryption(config,
        securityInfo.clientCertificate, securityInfo.privateKey,
        NULL, 0, NULL, 0);

#ifdef __linux__ /* UA_CertificateVerification_CertFolders supported only for Linux so far */
    if (securityCertificateTrustListDir.length() ||
        securityIssuersCertificatesDir.length()) {
        if (debug) {
            std::cout << "Session " << name
                      << ": (connect) setting up PKI provider"
                      << std::endl;
        }
        UA_StatusCode status = UA_CertificateVerification_CertFolders(
            &config->certificateVerification,
            securityCertificateTrustListDir.c_str(),
            securityIssuersCertificatesDir.c_str(),
            securityIssuersRevocationListDir.c_str());
        if (UA_STATUS_IS_BAD(status))
            errlogPrintf("OPC UA session %s: setting up PKI context failed with status %s\n",
                         name.c_str(), UA_StatusCode_name(status));
    }
#endif // #ifdef __linux__
#else // #ifdef HAS_SECURITY
    UA_ClientConfig_setDefault(config);
#endif
    config->clientDescription.applicationType = UA_APPLICATIONTYPE_CLIENT;
    config->clientDescription.applicationName = UA_LOCALIZEDTEXT_ALLOC("en-US", "EPICS IOC");
    config->clientDescription.productUri = UA_STRING_ALLOC("urn:EPICS:IOC");
    config->clientDescription.applicationUri = UA_STRING_ALLOC(applicationUri.c_str());

    config->outStandingPublishRequests = 5; // TODO: configure this as an option
    config->clientContext = this;

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

    /* connect status change callback */
    config->stateCallback = [] (UA_Client *client,
            UA_SecureChannelState channelState,
            UA_SessionState sessionState,
            UA_StatusCode connectStatus)
        {
            static_cast<SessionOpen62541*>(UA_Client_getContext(client))->
                connectionStatusChanged(channelState, sessionState, connectStatus);
        };

    config->securityMode = securityInfo.securityMode;
    UA_String_copy(&securityInfo.securityPolicyUri, &config->securityPolicyUri);
    UA_copy(&securityInfo.userIdentityToken, &config->userIdentityToken, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]);
    connectStatus = UA_Client_connect(client, serverURL.c_str());

    if (!UA_STATUS_IS_BAD(connectStatus)) {
        if (debug)
            std::cerr << "Session " << name
                      << ": connect service succeeded"
                      << std::endl;
    } else {
        if (manual || debug)
            errlogPrintf("OPC UA session %s: connect service failed with status %s\n",
                         name.c_str(),
                         UA_StatusCode_name(connectStatus));
        UA_Client_delete(client);
        client = nullptr;
        if (autoConnect)
           autoConnector.start();
        return -1;
    }
    // asynchronous: Remaining actions are done in connectionStatusChanged()
    // Use low prio because the thread needs to loop a lot, see run().
    workerThread = new epicsThread(*this, ("OPCrun-" + name).c_str(),
                 epicsThreadGetStackSize(epicsThreadStackSmall),
                 epicsThreadPriorityLow);
    workerThread->start();
    return 0;
}

long
SessionOpen62541::disconnect ()
{
    if (!client) {
        if (debug)
            std::cerr << "Session " << name
                    << " already disconnected"
                    << std::endl;
        return 0;
    }
    {
        Guard G(clientlock);
        if(!client) return 0;
        UA_Client_delete(client); // this also deletes all open62541 subscriptions
        client = nullptr;
    }
    // Worker thread terminates when client was destroyed
    if (workerThread) {
        workerThread->exitWait();
        delete workerThread;
        workerThread = nullptr;
    }
    return 0;
}

bool
SessionOpen62541::isConnected () const
{
    return client && sessionState == UA_SESSIONSTATE_ACTIVATED;
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
    if (!isConnected())
        return;

    UA_StatusCode status;
    std::unique_ptr<std::vector<ItemOpen62541 *>> itemsToRead(new std::vector<ItemOpen62541 *>);
    UA_UInt32 id = getTransactionId();
    UA_ReadRequest request;

    UA_ReadRequest_init(&request);
    request.maxAge = 0;
    request.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
    request.nodesToReadSize = batch.size();
    request.nodesToRead = static_cast<UA_ReadValueId*>(UA_Array_new(batch.size(), &UA_TYPES[UA_TYPES_READVALUEID]));

    UA_UInt32 i = 0;
    for (auto c : batch) {
        UA_NodeId_copy(&c->item->getNodeId(), &request.nodesToRead[i].nodeId);
        request.nodesToRead[i].attributeId = UA_ATTRIBUTEID_VALUE;
        itemsToRead->push_back(c->item);
        i++;
    }

    {
        Guard G(clientlock);
        if (!isConnected()) return; // may have disconnected while we waited
        status=UA_Client_sendAsyncReadRequest(client, &request,
            [] (UA_Client *client,
                void *userdata,
                UA_UInt32
                requestId,
                UA_ReadResponse *response)
            {
                static_cast<SessionOpen62541*>(userdata)->readComplete(requestId, response);
            },
            this, &id);
    }
    UA_ReadRequest_clear(&request);
    if (UA_STATUS_IS_BAD(status)) {
        errlogPrintf(
            "OPC UA session %s: (requestRead) beginRead service failed with status %s\n",
            name.c_str(),
            UA_StatusCode_name(status));
        // Create readFailure events for all items of the batch
        for (auto c : batch) {
            c->item->setIncomingEvent(ProcessReason::readFailure);
        }
    } else {
        if (debug >= 5)
            std::cout << "Session " << name
                      << ": (requestRead) beginRead service ok"
                      << " (transaction id " << id
                      << "; retrieving " << itemsToRead->size()
                      << " nodes)"
                      << std::endl;
        Guard G(opslock);
        outstandingOps.insert(
            std::pair<UA_UInt32,
                std::unique_ptr<std::vector<ItemOpen62541 *>>>(id, std::move(itemsToRead)));
    }
}

void
SessionOpen62541::requestWrite (ItemOpen62541 &item)
{
    auto cargo = std::make_shared<WriteRequest>();
    cargo->item = &item;
    item.copyAndClearOutgoingData(cargo->wvalue);
    writer.pushRequest(cargo, item.recConnector->getRecordPriority());
}

// Low level writer function called by the RequestQueueBatcher
void
SessionOpen62541::processRequests (std::vector<std::shared_ptr<WriteRequest>> &batch)
{
    if (!isConnected())
        return;

    UA_StatusCode status;
    std::unique_ptr<std::vector<ItemOpen62541 *>> itemsToWrite(new std::vector<ItemOpen62541 *>);
    UA_UInt32 id = getTransactionId();
    UA_WriteRequest request;

    UA_WriteRequest_init(&request);
    request.nodesToWriteSize = batch.size();
    request.nodesToWrite = static_cast<UA_WriteValue*>(UA_Array_new(batch.size(), &UA_TYPES[UA_TYPES_WRITEVALUE]));

    UA_UInt32 i = 0;
    for (auto c : batch) {
        UA_NodeId_copy(&c->item->getNodeId(), &request.nodesToWrite[i].nodeId);
        request.nodesToWrite[i].attributeId = UA_ATTRIBUTEID_VALUE;
        request.nodesToWrite[i].value.hasValue = true;
        request.nodesToWrite[i].value.value = c->wvalue.value.value;
        itemsToWrite->push_back(c->item);
        i++;
    }

    {
        Guard G(clientlock);
        if (!isConnected()) return; // may have disconnected while we waited
        status=UA_Client_sendAsyncWriteRequest(client, &request,
            [] (UA_Client *client,
                void *userdata,
                UA_UInt32 requestId,
                UA_WriteResponse *response)
            {
                static_cast<SessionOpen62541*>(userdata)->writeComplete(requestId, response);
            },
            this, &id);
    }

    UA_WriteRequest_clear(&request);
    if (UA_STATUS_IS_BAD(status)) {
        errlogPrintf("OPC UA session %s: (requestWrite) beginWrite service failed with status %s\n",
                     name.c_str(), UA_StatusCode_name(status));
        // Create writeFailure events for all items of the batch
        for (auto c : batch) {
            c->item->setIncomingEvent(ProcessReason::writeFailure);
        }
    } else {
        if (debug >= 5)
            std::cout << "Session " << name
                      << ": (requestWrite) beginWrite service ok"
                      << " (transaction id " << id
                      << "; writing " << itemsToWrite->size()
                      << " nodes)"
                      << std::endl;
        Guard G(opslock);
        outstandingOps.insert(std::pair<UA_UInt32,
            std::unique_ptr<std::vector<ItemOpen62541 *>>>(id, std::move(itemsToWrite)));
    }
}

void
SessionOpen62541::createAllSubscriptions ()
{
    for (auto &it : subscriptions) {
        it.second->create();
    }
}

void
SessionOpen62541::addAllMonitoredItems ()
{
    for (auto &it : subscriptions) {
        it.second->addMonitoredItems();
    }
}

void
SessionOpen62541::registerNodes ()
{
    UA_RegisterNodesRequest request;
    UA_RegisterNodesRequest_init(&request);

    request.nodesToRegister = static_cast<UA_NodeId*>(UA_Array_new(items.size(), &UA_TYPES[UA_TYPES_NODEID]));
    UA_UInt32 i = 0;
    for (auto &it : items) {
        if (it->linkinfo.registerNode) {
            it->show(0);
            UA_NodeId_copy(&it->getNodeId(), &request.nodesToRegister[i]);
            i++;
        }
    }
    registeredItemsNo = i;
    request.nodesToRegisterSize = registeredItemsNo;

    if (registeredItemsNo) {
        UA_RegisterNodesResponse response = UA_Client_Service_registerNodes(client, request);
        if (UA_STATUS_IS_BAD(response.responseHeader.serviceResult)) {
            errlogPrintf("OPC UA session %s: (registerNodes) registerNodes service failed with status %s\n",
                         name.c_str(), UA_StatusCode_name(response.responseHeader.serviceResult));
        } else {
            if (debug)
                std::cout << "Session " << name
                          << ": (registerNodes) registerNodes service ok"
                          << " (" << response.registeredNodeIdsSize
                          << " nodes registered)"
                          << std::endl;
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
    UA_RegisterNodesRequest_clear(&request);
}

void
SessionOpen62541::rebuildNodeIds ()
{
    for (auto &it : items)
        it->rebuildNodeId();
}

/* Add a mapping to the session's map, replacing any existing mappings with the same
 * index or URI */
void
SessionOpen62541::addNamespaceMapping (const unsigned short nsIndex, const std::string &uri)
{
    errlogPrintf("SessionOpen62541::addNamespaceMappingindex %u = %s\n", nsIndex, uri.c_str());
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
SessionOpen62541::updateNamespaceMap(const UA_String *nsArray, UA_UInt16 nsCount)
{
    if (debug)
        std::cout << "Session " << name
                  << ": (updateNamespaceMap) namespace array with " << nsCount
                  << " elements read; updating index map with " << namespaceMap.size()
                  << " entries" << std::endl;
    if (namespaceMap.size()) {
        nsIndexMap.clear();
        for (UA_UInt16 i = 0; i < nsCount; i++) {
            std::string ns;
            ns += nsArray[i];
            auto it = namespaceMap.find(ns);
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
SessionOpen62541::showSecurity ()
{
    UA_StatusCode status;
    UA_ApplicationDescription* applicationDescriptions;
    size_t applicationDescriptionsLength;

    UA_Client* discovery = UA_Client_new();
    UA_ClientConfig *config = UA_Client_getConfig(discovery);
    UA_ClientConfig_setDefault(config);

    status = UA_Client_findServers(discovery, serverURL.c_str(),
        0, NULL, 0, NULL,
        &applicationDescriptionsLength, &applicationDescriptions);
    if (UA_STATUS_IS_BAD(status)) {
        std::cerr << "Session " << name << ": (showSecurity) UA_Client_findServers failed"
                  << " with status " << UA_StatusCode_name(status)
                  << std::endl;
        UA_Client_delete(discovery);
        return;
    }

#ifndef HAS_SECURITY
    std::cout << "Client library does not support security features." << std::endl;
#endif

    setupClientSecurityInfo(securityInfo, &name, debug);
    setupIdentity();

    for (size_t i = 0; i < applicationDescriptionsLength; i++) {
        for (size_t j = 0; j < applicationDescriptions[i].discoveryUrlsSize; j++) {
            std::cout << "Session " << name << " (discovery at " << serverURL << ")"
                      << "\n  Server Name: " << applicationDescriptions[i].applicationName
                      << "\n  Server URI:  " << applicationDescriptions[i].applicationUri
                      << "\n  Server URL:  " << applicationDescriptions[i].discoveryUrls[j];
            if (serverURL != applicationDescriptions[i].discoveryUrls[j])
                std::cout << "    (using " << serverURL << ")";
            std::cout << "\n  Requested Mode: " << reqSecurityMode
                      << "    Policy: " << Session::securityPolicyString(reqSecurityPolicyUri)
                      << "\n  Identity: ";
            if (config->userIdentityToken.encoding == UA_EXTENSIONOBJECT_DECODED &&
                config->userIdentityToken.content.decoded.type == &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN]) {
                UA_UserNameIdentityToken* identityToken =
                    static_cast<UA_UserNameIdentityToken*>(config->userIdentityToken.content.decoded.data);
                std::cout << "Username token '" << identityToken->userName << "'"
                          << " (credentials from " << securityIdentityFile << ")";
            }
            else
                std::cout << "Anonymous";

            if (serverURL.compare(0, 7, "opc.tcp") == 0) {
                UA_EndpointDescription* endpointDescriptions;
                size_t endpointDescriptionsLength;
                status = UA_Client_getEndpoints(discovery, serverURL.c_str(),
                    &endpointDescriptionsLength, &endpointDescriptions);
                if (UA_STATUS_IS_BAD(status)) {
                    std::cout << std::endl;
                    std::cerr << "Session " << name << ": (showSecurity) UA_Client_getEndpoints failed"
                              << " with status" << UA_StatusCode_name(status)
                              << std::endl;
                    UA_Client_delete(discovery);
                    return;
                }

                for (size_t k = 0; k < endpointDescriptionsLength; k++) {
                    if (std::string(reinterpret_cast<const char*>(endpointDescriptions[k].endpointUrl.data),
                        endpointDescriptions[k].endpointUrl.length).compare(0, 7, "opc.tcp") == 0) {
                        char dash = '-';
                        std::string marker;
                        if (isConnected()
                            && endpointDescriptions[k].securityMode
                                   == securityInfo.securityMode
                            && UA_String_equal(&endpointDescriptions[k].securityPolicyUri,
                                   &securityInfo.securityPolicyUri)) {
                            dash = '=';
                            marker = " connected =====";
                        }
                        std::cout << "\n  " << std::setfill(dash) << std::setw(5) << dash
                                  << " Endpoint " << k << " " << std::setw(5) << dash
                                  << std::setfill(' ') << " Level: " << std::setw(3)
                                  << +endpointDescriptions[k].securityLevel << " "
                                  << std::setfill(dash) << std::setw(50) << dash
                                  << std::setfill(' ')  << marker
                                  << "\n    Security Mode: " << std::setw(14) << std::left
                                  << endpointDescriptions[k].securityMode
                                  << std::right << "    Policy: "
                                  << securityPolicyString(
                                        endpointDescriptions[k].securityPolicyUri)
                                  << "\n    URL: "
                                  << endpointDescriptions[k].endpointUrl;
                        if (UA_String_equal(&endpointDescriptions[k].endpointUrl, &applicationDescriptions[i].discoveryUrls[j]))
                            std::cout << "    (using " << serverURL << ")";

                        UA_ByteString_copy(&endpointDescriptions[k].serverCertificate, &securityInfo.serverCertificate);
#ifdef HAS_SECURITY
/* TODO: Certificate parsing not implemented in Open62541
   Use openssl directly?
                        UA_ByteString cert = securityInfo.serverCertificate;
                        UaPkiIdentity id = cert.subject();
                        std::cout << "\n    Server Certificate: " << id.commonName
                                  << " (" << id.organization << ")"
                                  << " serial " << cert.serialNumber()
                                  << " (thumb " << cert.thumbPrint().toHex(false) << ")"
                                  << (cert.isSelfSigned() ? " self-signed" : "")
                                  << (securityInfo.verifyServerCertificate().isBad() ? " - not" : " -")
                                  << " trusted";
*/
#endif
                    }
                }
                std::cout << std::endl;
                UA_Array_delete(endpointDescriptions, endpointDescriptionsLength, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
            }
        }
    }
    UA_Client_delete(discovery);
}

SessionOpen62541::ConnectResult
SessionOpen62541::setupSecurity ()
{
#ifdef HAS_SECURITY
    if (reqSecurityMode == RequestedSecurityMode::None) {
#endif
        securityInfo.securityMode = UA_MESSAGESECURITYMODE_NONE;
        UA_String_clear(&securityInfo.securityPolicyUri);
        securityLevel = 0;
        UA_ByteString_clear(&securityInfo.serverCertificate);
        setupIdentity();

        if (debug)
            std::cout << "Session " << name
                      << ": (setupSecurity) no security configured "
                      << std::endl;
        return ConnectResult::ok;
#ifdef HAS_SECURITY
    } else {
        setupIdentity();
        if (std::string(serverURL).compare(0, 7, "opc.tcp") == 0) {
            UA_ClientConfig *config = UA_Client_getConfig(client);
            UA_EndpointDescription* endpointDescriptions;
            size_t endpointDescriptionsLength;
            if (debug)
                std::cout << "Session " << name
                          << ": (setupSecurity) reading endpoints from " << serverURL
                          << std::endl;

            connectStatus = UA_Client_getEndpoints(client, serverURL.c_str(),
                    &endpointDescriptionsLength, &endpointDescriptions);
            if (UA_STATUS_IS_BAD(connectStatus)) {
                if (debug)
                    std::cout << "Session " << name
                              << ": (setupSecurity) UaDiscovery::getEndpoints from " << serverURL
                              << " failed with status "
                              << UA_StatusCode_name(connectStatus)
                              << std::endl;
                return ConnectResult::cantConnect;
            }

            int selectedSecurityLevel = -1;
            int selectedEndpoint = -1;
            for (size_t k = 0; k < endpointDescriptionsLength; k++) {
                if (std::string(reinterpret_cast<const char*>(endpointDescriptions[k].endpointUrl.data),
                        endpointDescriptions[k].endpointUrl.length).compare(0, 7, "opc.tcp") == 0) {
                    if (reqSecurityMode == RequestedSecurityMode::Best ||
                        OpcUaSecurityMode(reqSecurityMode) == endpointDescriptions[k].securityMode) {
                        if (reqSecurityPolicyUri.find("#None") != std::string::npos ||
                            reqSecurityPolicyUri == endpointDescriptions[k].securityPolicyUri) {
                            if (endpointDescriptions[k].securityLevel > selectedSecurityLevel) {
                                size_t l;
                                for (l = 0; l < config->securityPoliciesSize; l++) {
                                    if (UA_String_equal(&config->securityPolicies[l].policyUri, &endpointDescriptions[k].securityPolicyUri)) {
                                        selectedEndpoint = k;
                                        selectedSecurityLevel = endpointDescriptions[k].securityLevel;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (selectedEndpoint >= 0) {
                securityInfo.securityMode = endpointDescriptions[selectedEndpoint].securityMode;
                UA_String_clear(&securityInfo.securityPolicyUri);
                UA_String_copy(&endpointDescriptions[selectedEndpoint].securityPolicyUri, &securityInfo.securityPolicyUri);
                UA_ByteString_clear(&securityInfo.serverCertificate);
                UA_ByteString_copy(&endpointDescriptions[selectedEndpoint].serverCertificate, &securityInfo.serverCertificate);
                securityLevel = endpointDescriptions[selectedEndpoint].securityLevel;

                // Verify server certificate (and save if rejected and securitySaveRejected is set)
/* TODO: Implement verification
   Be trusty for now. Open62541 will reject servers with invalid certificates at connect
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
                                          << UaString(rejectedDir.toUtf16()) << ")" << std::endl;
                                save = false;
                            }
                        }
                        if (save) {
                            std::string cleanCN(
                                replaceInvalidFilenameChars(id.commonName).c_str());
                            UaString fileName = UaString("%1/%2 [%3].der")
                                                    .arg(securitySaveRejectedDir.c_str())
                                                    .arg(cleanCN.c_str())
                                                    .arg(cert.thumbPrint().toHex());
                            UaUniString uniFileName(UaDir::fromNativeSeparators(fileName.toUtf16()));
                            if (!dirHelper.exists(uniFileName)) {
                                int status = cert.toDERFile(
                                    UaDir::toNativeSeparators(uniFileName).toLocal8Bit());
                                if (status)
                                    std::cout << "Session " << name
                                              << ": (setupSecurity) ERROR - cannot save rejected "
                                                 "certificate as "
                                              << fileName << std::endl;
                                else
                                    std::cout << "Session " << name
                                              << ": (setupSecurity) saved rejected certificate as "
                                              << UaString(uniFileName.toUtf16()) << std::endl;
                            }
                        }
                    }
                } else
*/
                {
                    if (debug)
                        std::cout << "Session " << name
                                  << ": (setupSecurity) found matching endpoint number " << selectedEndpoint
                                  << ", using mode="
                                  << endpointDescriptions[selectedEndpoint].securityMode
                                  << " policy="
                                  << securityPolicyString(endpointDescriptions[selectedEndpoint].securityPolicyUri)
                                  << " (level " << +securityLevel << ")" << std::endl;
                }
            } else {
                if (debug)
                    std::cout << "Session " << name
                              << ": (setupSecurity) found no endpoint that matches"
                              << " the security requirements" << std::endl;
                UA_Array_delete(endpointDescriptions, endpointDescriptionsLength, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
                return ConnectResult::noMatchingEndpoint;
            }
            UA_Array_delete(endpointDescriptions, endpointDescriptionsLength, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
            return ConnectResult::ok;
        } else {
            errlogPrintf("OPC UA session %s: fatal - only URLs of type 'opc.tcp' supported\n",
                         name.c_str());
            return SessionOpen62541::fatal;
        }
    }
    return ConnectResult::ok;
#endif // #ifdef HAS_SECURITY
}

void SessionOpen62541::setupClientSecurityInfo(ClientSecurityInfo &securityInfo,
                                      const std::string *sessionName,
                                      const int debug)
{
#ifdef HAS_SECURITY
    if (securityClientCertificateFile.length() && securityClientPrivateKeyFile.length()) {
        if (debug) {
            if (sessionName)
                std::cout << "Session " << *sessionName;
            std::cout << ": (initClientSecurity) loading client certificate "
                      << securityClientCertificateFile
                      << std::endl;
        }
        UA_ByteString cert = loadFile(securityClientCertificateFile.c_str());
        if (cert.length == 0) {
            errlogPrintf("%s%s: loading client certificate %s failed\n",
                         sessionName ? "OPC UA Session " : "OPC UA",
                         sessionName ? sessionName->c_str() : "",
                         securityClientCertificateFile.c_str());
            return;
        }
/* TODO: Implement certificate validity check
        if (!cert_is_valid()) {
            errlogPrintf("%s%s: configured client certificate is not valid (expired?)\n",
                         sessionName ? "OPC UA Session " : "OPC UA",
                         sessionName ? sessionName->c_str() : "");
            return;
        }
*/
        if (debug) {
            if (sessionName)
                std::cout << "Session " << *sessionName;
            std::cout << ": (initClientSecurity) loading client private key "
                      << securityClientPrivateKeyFile
                      << std::endl;
        }
        UA_ByteString key;
        key = loadFile(securityClientPrivateKeyFile.c_str());
        if (key.length == 0) {
            errlogPrintf("%s%s: loading client private key %s failed\n",
                         sessionName ? "OPC UA Session " : "OPC UA",
                         sessionName ? sessionName->c_str() : "",
                         securityClientPrivateKeyFile.c_str());
            return;
        }
/* TODO: Implement check if key matches certificate
        if (!cert_and_key_match(cert, key)) {
            errlogPrintf("%s%s: client certificate and client key don't match\n",
                         sessionName ? "OPC UA Session " : "OPC UA",
                         sessionName ? sessionName->c_str() : "");
            return;
        }
*/
        securityInfo.clientCertificate = cert;
        securityInfo.privateKey = key;
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
SessionOpen62541::show (const int level) const
{
    std::cout << "session="      << name
              << " url="         << serverURL
              << " connect status=" << UA_StatusCode_name(connectStatus)
              << " sessionState="   << sessionState
              << " channelState="   << channelState
              << " sec-mode="    << securityInfo.securityMode
              << "(" << reqSecurityMode << ")"
              << " sec-policy="  << securityPolicyString(securityInfo.securityPolicyUri)
              << "(" << reqSecurityPolicyUri << ")"
              << " debug="       << debug
              << " batch r/w="   << MaxNodesPerRead << "/" << MaxNodesPerWrite
              << "(" << readNodesMax << "/" << writeNodesMax << ")"
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

inline void
SessionOpen62541::markConnectionLoss()
{
    reader.clear();
    writer.clear();
    for (auto it : items) {
        it->setState(ConnectionStatus::down);
        it->setIncomingEvent(ProcessReason::connectionLoss);
    }
}

void
SessionOpen62541::setupIdentity()
{
    UA_ExtensionObject_clear(&securityInfo.userIdentityToken);
    securityUserName = "Anonymous";

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
                std::cout << "Session " << name
                          << ": (setupIdentity) setting Username token (" << user
                          << "/*****)" << std::endl;
            securityUserName = user;
            UA_UserNameIdentityToken* identityToken = UA_UserNameIdentityToken_new();
            if (!identityToken) {
                errlogPrintf(
                    "OPC UA session %s: out of memory",
                    name.c_str());
                return;
            }
            identityToken->userName = UA_STRING_ALLOC(user.c_str());
            identityToken->password = UA_STRING_ALLOC(pass.c_str());
            securityInfo.userIdentityToken.encoding = UA_EXTENSIONOBJECT_DECODED;
            securityInfo.userIdentityToken.content.decoded.type = &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN];
            securityInfo.userIdentityToken.content.decoded.data = identityToken;

        } else if (certfile.length() && keyfile.length()) {
            if (debug)
                std::cout << "Session " << name
                          << ": (setupIdentity) loading identity certificate "
                          << certfile << std::endl;

            UA_ByteString cert = loadFile(certfile.c_str());
            if (cert.length == 0) {
                errlogPrintf("OPC UA Session %s: loading client certificate %s failed: %s\n",
                             name.c_str(), certfile.c_str(), strerror(errno));
                return;
            }
            if (client) {
            /* TODO: certificate verification before connect(), when client
               does not yet exist!
               Even if we cannot verify here, UA_Client_connect will verify.
            */
                UA_ClientConfig *config = UA_Client_getConfig(client);
                if (!config->certificateVerification.verifyCertificate) {
                    errlogPrintf("OPC UA session %s: No certificate validation support available",
                                name.c_str());
                } else {
                    UA_StatusCode status = config->certificateVerification.verifyCertificate(
                        config->certificateVerification.context, &cert);
                    if (UA_STATUS_IS_BAD(connectStatus)) {
                        errlogPrintf("OPC UA session %s: identity certificate is not valid: %s\n",
                                    name.c_str(), UA_StatusCode_name(status));
                        return;
                    }
                }
            }
            UA_X509IdentityToken* identityToken = UA_X509IdentityToken_new();
            if (!identityToken) {
                errlogPrintf(
                    "OPC UA session %s: out of memory",
                    name.c_str());
                return;
            }
            securityUserName = "Certificate user";
            // TODO: get name from certificate
            identityToken->certificateData = cert;
            // needed? UA_String_copy(&certificate_policy, &identityToken->policyId);
            securityInfo.userIdentityToken.encoding = UA_EXTENSIONOBJECT_DECODED;
            securityInfo.userIdentityToken.content.decoded.type = &UA_TYPES[UA_TYPES_X509IDENTITYTOKEN];
            securityInfo.userIdentityToken.content.decoded.data = identityToken;
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
            std::cout << "Session " << name << ": (setupIdentity) setting Anonymous token"
                      << std::endl;
    }
}

void
SessionOpen62541::run ()
{
    // Currently (open62541 version 1.3), the client has no internal mechanism
    // to run asynchronous tasks. We need to create our own thread to call
    // UA_Client_run_iterate() repeatedly for asynchronous events to happen.
    // Also until now, the client is not thread save. We have to use our own
    // mutex (clientlock) to synchonize access to the client object from
    // different threads. These are in particular this worker thread, which
    // calls the callbacks connectionStatusChanged() and read/writeComplete(),
    // the batcher threads, which call processRequests(), and the iocsh.
    // Unfortunately, there is no way to release the mutex while
    // UA_Client_run_iterate() waits for incoming network traffic.
    // Thus use a short timeout and sleep without holding the mutex.
    std::cerr << "Session " << name
              << " worker thread start"
              << std::endl;
    Guard G(clientlock);
    while (true)
    {
        if (!client) {
             std::cerr << "Session " << name
                       << " worker thread: Client destroyed. Finishing."
                       << std::endl;
            return;
        }
        connectStatus = UA_Client_run_iterate(client, 1);
        {
            UnGuard U(G);
            epicsThreadSleep(0.01); // give other threads a chance to execute
        }
        if (client && UA_STATUS_IS_BAD(connectStatus))
            break;
    }
    std::cerr << "Session " << name
              << " worker thread error: connectStatus:"
              << UA_StatusCode_name(connectStatus)
              << " sessionState:" << sessionState
              << " channelState:" << channelState
              << std::endl;
    disconnect();
}

// This function collects reference type ID, forwarded flag,
// Browse name, display name, node class and type definition
// Retuns true if browse was successful
UA_StatusCode browseNodeId(UA_Client* client, UA_NodeId nodeId, UA_BrowseResponse* bResp) {
    if (!client) {
        return UA_STATUSCODE_BAD;
    }
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    UA_NodeId_copy(&nodeId, &bReq.nodesToBrowse[0].nodeId);
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_BOTH;
    *bResp = UA_Client_Service_browse(client, bReq);
    UA_BrowseRequest_clear(&bReq);
    return UA_STATUSCODE_GOOD;
}

#define UA_NAMESPACEOFFSET 1000 // the name space offset is used to read data types with the same name space index from different dictionaries
std::map<UA_UInt32, xmlDocPtr> xmlDocMap;

inline const char*
nodeName(xmlNode* node)
{
    return reinterpret_cast<const char*>(node->name);
}

inline const char*
getProp(xmlNode* node, const char* name)
{
    return reinterpret_cast<const char*>(xmlGetProp(node, reinterpret_cast<const xmlChar*>(name)));
}

const UA_DataType*
getTypeByName(const char* typeName, const UA_DataTypeArray *customTypes)
{
#ifdef UA_ENABLE_TYPEDESCRIPTION
// Without type description, we cannot find any member types by name.

    if (((strncmp(typeName, "opc:", 4) == 0) && (typeName += 4)) ||
        ((strncmp(typeName, "ua:", 3) == 0) && (typeName += 3))) {
        // Search in built-in types.
        for (size_t i = 0; i < UA_TYPES_COUNT; i++) {
            if (strcmp(typeName, UA_TYPES[i].typeName) == 0)
                return &UA_TYPES[i];
        }
        if (strcmp(typeName, "CharArray") == 0)
            return &UA_TYPES[UA_TYPES_STRING];
    }
    else
    {
        // Search in custom type definitions.
        bool tns = false;
        if (strncmp(typeName, "tns:", 4) == 0 && (typeName += 4))
            tns = true;
        while (customTypes)
        {
            for (size_t i = 0; i < customTypes->typesSize; i++) {
                if (!customTypes->types[i].typeName)
                    continue;
                if (strcmp(typeName, customTypes->types[i].typeName) == 0)
                    return &customTypes->types[i];
            }
            if (tns) {
                // tns="Target Name Space: search only our current,
                // i.e. the first type array
                break;
            }
            customTypes = customTypes->next;
        }
    }
#endif
    return nullptr;
}

void
addUserDataTypes(xmlNode* node, UA_UInt16 nsIndex, UA_DataTypeArray **customTypes, int depth = 0)
{
#ifdef UA_ENABLE_TYPEDESCRIPTION
    std::vector<UA_DataType> userTypes;
    struct unresolvedType {size_t typeIndex; size_t memberIndex; const char* typeName;};
    std::vector<unresolvedType> unresolvedTypes;
    UA_DataTypeArray currentCustomTypes = {*customTypes, 0, nullptr};

    for (;node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE)
            continue; // in particular ignore all text elements

        // Traverse the xml document tree until we find something interesting.

        printf("%3d: %*cnode %s\n", node->line, depth, ' ', nodeName(node));
        if (node->content)
            printf("%3d:  %*ccontent: %s\n", node->line, depth, ' ', node->content);
        for (xmlAttr* prop = node->properties; prop; prop = prop->next) {
            printf("      %*cprop %s = \"%s\"\n", depth, ' ', prop->name, prop->children->content);
        }
        addUserDataTypes(node->children, nsIndex, customTypes, depth+1);

        UA_DataType userDataType = {};
        userDataType.typeKind = UA_DATATYPEKIND_STRUCTURE; // Until proven otherwise
        userDataType.pointerFree = true;
#ifndef UA_DATATYPES_USE_POINTER
        userDataType.typeIndex = static_cast<UA_UInt16>(userTypes.size());
#endif
        std::vector<UA_DataTypeMember> members;
        const char* userTypeKind = nodeName(node);
        const char* typeName = getProp(node, "Name");

        if (strcmp(userTypeKind, "StructuredType") == 0) {
            if (!typeName) {
                 std::cerr << "unexpected " << userTypeKind << " without name" << std::endl;
                 continue;
            }
            UA_UInt32 structureAlignment = 0;
            UA_UInt32 memberSize;

            const char* baseType = getProp(node, "BaseType");
            if (baseType && strcmp(baseType, "ua:Union") == 0)
                userDataType.typeKind = UA_DATATYPEKIND_UNION;

            std::cout << "\n" << (userDataType.typeKind == UA_DATATYPEKIND_UNION ? "union" : "struct")
                      <<  " " << typeName << " {"
                      <<  " # tns:" << userTypes.size()
                      << std::endl;

            // Get the fields.
            for (xmlNode* field = node->children; field; field = field->next) {
                if (field->type != XML_ELEMENT_NODE)
                    continue;
                if (strcmp(nodeName(field), "Documentation") == 0) {
                    for (xmlNode* doc = field->children; doc; doc = doc->next) {
                        if (doc->content)
                            std::cout << "# " << doc->content << std::endl;
                    }
                    continue;
                }
                if (strcmp(nodeName(field), "Field") != 0) {
                    std::cerr << "# unexpected node " <<  nodeName(field) << std::endl;
                    continue;
                }

                const char* fieldName = getProp(field, "Name");
                if (!fieldName) {
                    std::cerr << "# unexpected Field node without field name" << std::endl;
                    continue;
                }

                const char* fieldTypeName = getProp(field, "TypeName");
                if (!fieldName) {
                    std::cerr << "# unexpected field " << fieldName << " without type name" << std::endl;
                    continue;
                }

                if (userDataType.memSize == 0 &&
                    userDataType.typeKind != UA_DATATYPEKIND_UNION &&
                    strcmp(fieldTypeName, "opc:Bit") == 0) {
                    // Bit fields at the beginning of stuctures are part of
                    // a mask of optional fields.
                    // They are not stored explicitly
                    userDataType.typeKind = UA_DATATYPEKIND_OPTSTRUCT;
                    const char* length = getProp(field, "Length");
                    std::cout << "  # " << fieldTypeName
                              << " " << fieldName;
                    if (length)
                        std::cout << ":" << length;
                    std::cout << " # implicit" << std::endl;
                    continue;
                }

                UA_DataTypeMember member = {};
                const UA_DataType *memberType = getTypeByName(fieldTypeName, &currentCustomTypes); // May return nullptr!

#ifdef UA_DATATYPES_USE_POINTER
                member.memberType = memberType;
#define memberTypeOf(field) ((field).memberType)
#else
#define UNKNOWN_TYPE static_cast<UA_UInt16>(~0)
                member.namespaceZero = memberType >= &UA_TYPES[0] && memberType < &UA_TYPES[UA_TYPES_COUNT];
                member.memberTypeIndex = memberType ? memberType->typeIndex : UNKNOWN_TYPE;
#define memberTypeOf(field) ((field).memberTypeIndex == UNKNOWN_TYPE ? nullptr : ((field).namespaceZero ? UA_TYPES : currentCustomTypes.types)+(field).memberTypeIndex)
#endif

                if (userDataType.memSize == 0 &&
                    userDataType.typeKind == UA_DATATYPEKIND_UNION)
                {
                    // First field of a union must be UInt32 switch field,
                    if (memberType != &UA_TYPES[UA_TYPES_UINT32])
                    {
                        std::cerr << "addUserDataTypes: union " << typeName
                                  << " has switch field of unexpected type "
                                  << (memberType ? memberType->typeName : fieldTypeName)
                                  << " instead of Unt32"
                                  << std::endl;
                        break;
                    }
                    // It is not added to members but included in padding.
                    userDataType.memSize = sizeof(UA_UInt32);
                    std::cout << "  # " << memberType->typeName
                              << " " << fieldName
                              << "; # in padding"
                              << std::endl;
                    continue;
                }

                member.memberName = strdup(fieldName);

                const char* lengthFieldName = getProp(field, "LengthField");
                const char* switchFieldName = getProp(field, "SwitchField");

/*
                printf("# %s", fieldTypeName);
                if (lengthFieldName) {
                    printf("[%s]", lengthFieldName);
                }
                if (switchFieldName) {
                    printf(" {%s", switchFieldName);
                    const char* switchVal = getProp(field, "SwitchValue");
                    if (switchVal) {
                        printf(" == %s", switchVal);
                    }
                    printf(" ?}");
                }
                printf(" %s;\n", fieldName);
*/

                if (lengthFieldName) {
                    // Fields with length field are (mandatory or optional) arrays.
                    // First remove already added Int32 lengthField from members.
                    // It should be the previous member.
                    // We could not know that it was the length field until now.
                    {
                        UA_DataTypeMember& lengthField = members.back();
                        if (members.empty() || strcmp(lengthField.memberName, lengthFieldName) != 0) {
                            // previous field was not the length field!
                            std::cerr << "addUserDataTypes: struct " << typeName
                                      << " has array field " << fieldTypeName
                                      << " not immediately following its length field " << lengthFieldName
                                      << std::endl;
                            userDataType.memSize = 0;
                            break;
                        }
                        if (memberTypeOf(lengthField) != &UA_TYPES[UA_TYPES_INT32]) {
                            std::cerr << "addUserDataTypes: struct " << typeName
                                      << " has array " << fieldTypeName
                                      << " with length field " << lengthFieldName
                                      << " of unexpected type "
                                      << (memberTypeOf(lengthField) ? memberTypeOf(lengthField)->typeName : "Unknown")
                                      << " instead of Int32"
                                      << std::endl;
                            userDataType.memSize = 0;
                            break;
                        }
                        userDataType.memSize -= lengthField.padding +
                            lengthField.isOptional ? sizeof(void*) : memberTypeOf(lengthField)->memSize;
                        std::cout << "  # removed array length field " << memberTypeOf(lengthField)->typeName
                                  << " " << lengthField.memberName
                                  << " memberSize=" << (lengthField.isOptional ? sizeof(void*) : memberTypeOf(lengthField)->memSize)
                                  << " padding=" << static_cast<unsigned int>(lengthField.padding)
                                  << " memSize=" << userDataType.memSize
                                  << std::endl;
                        free(const_cast<char*>(lengthField.memberName));
                        members.pop_back();
                    }

                    // Arrays are stored as {size_t length; void* data;}.
                    userDataType.pointerFree = false;
                    memberSize = sizeof(size_t) + sizeof(void*);
                    member.isArray = true;
                    if (switchFieldName && userDataType.typeKind != UA_DATATYPEKIND_UNION)
                        member.isOptional = true;
                    std::cout << "  size_t " << fieldName
                              << "_Size;\n  " << (memberType ? memberType->typeName : fieldTypeName)
                              << "* " << fieldName
                              << "; # " << (member.isOptional ? "optional" : "mandatory")
                              << " " << fieldTypeName
                              << " array" << std::endl;
                }
                else if (switchFieldName && userDataType.typeKind != UA_DATATYPEKIND_UNION) {
                    // Structure fields with switch field are optional.
                    // They are stored as pointers.
                    userDataType.pointerFree = false;
                    memberSize = sizeof(void*);
                    member.isOptional = true;
                    std::cout << "  " << (memberType ? memberType->typeName : fieldTypeName)
                              << "* " << fieldName
                              << "; # optional " << fieldTypeName
                              << std::endl;
                }
                else {
                    // scalars are stored in-place
                    if (!memberType) {
                        // Sometimes, referenced types are defined after use.
                        // We can toletate that for arrays and optional fields
                        // where we only deal with pointers,
                        // but here we need the type size.
                        std::cerr << "addUserDataTypes: member type " << fieldTypeName
                                  << " of member " << fieldName
                                  << " in " << typeName
                                  << " undefined"
                                  << std::endl;
                        userDataType.memSize = 0;
                        break;
                    }
                    memberSize = memberType->memSize;
                    userDataType.pointerFree = userDataType.pointerFree && memberType->pointerFree;
                    std::cout << "  " << memberType->typeName
                              << " " << fieldName
                              << " # mandatory " << fieldTypeName
                              << std::endl;
                }

                // Pad to align this member.
                // Primitives are max 8 bytes long (double, void*, int64).
                // Thus, maximal 8 byte alignment is sufficient.
                UA_UInt32 memberAlignment = (memberSize-1) & 7; // actually alignment-1
                if (userDataType.typeKind != UA_DATATYPEKIND_UNION) {
                    // Align this member on top of structure so far.
                    member.padding = memberAlignment & ~(userDataType.memSize-1);
                    // Total size is the sum of all member sizes including padding.
                    userDataType.memSize += member.padding + memberSize;
                } else {
                    // For unions padding includes the UInt32 switch field.
                    member.padding = sizeof(UA_UInt32) + (memberAlignment & ~(sizeof(UA_UInt32)-1));
                    // Total size is the maximum of all member sizes including padding.
                    if (member.padding + memberSize > userDataType.memSize)
                        userDataType.memSize = member.padding + memberSize;
                }
                // Align structure to largest member alignment.
                if (memberAlignment > structureAlignment)
                    structureAlignment = memberAlignment;
                std::cout << "  # memberSize=" << memberSize
                          << " alignment=" << memberAlignment+1
                          << " padding=" << static_cast<unsigned int>(member.padding)
                          << " memSize=" << userDataType.memSize
                          << std::endl;

                if (!memberType) {
                    // Sometimes, referenced types are defined after use.
                    std::cerr << "# unresolved type " << userTypes.size() << '=' << typeName << " member " << members.size() << '=' << fieldName << " type=" << fieldTypeName << std::endl;
                    unresolvedTypes.push_back({userTypes.size(), members.size(), fieldTypeName});
                }
                members.push_back(member);
            }

            if (!userDataType.memSize) // Error bail out: Clean up and ignore invalid types.
            {
                for (auto it: members)
                    free(const_cast<char*>(it.memberName));
                members.clear();
                continue;
            }

            // Pad structure to align with its largest primitive.
            userDataType.memSize += (structureAlignment & ~(userDataType.memSize-1));
            std::cout << "}; # alignment=" << structureAlignment+1
                      << " memSize=" << userDataType.memSize
                      << " " << members.size() << " members"
                      << std::endl;

            // Move collected members into userDataType.
            userDataType.membersSize = members.size();
            userDataType.members = static_cast<UA_DataTypeMember*>(malloc(userDataType.membersSize * sizeof(UA_DataTypeMember)));
            memcpy(userDataType.members, members.data(), userDataType.membersSize * sizeof(UA_DataTypeMember));
        }

        if (strcmp(userTypeKind, "EnumeratedType") == 0) {
            if (!typeName) {
                 std::cerr << "unexpected " << userTypeKind << " without name" << std::endl;
                 continue;
            }
            // Enums are stored as UInt32 index
            const char* enumSizeStr = getProp(node, "LengthInBits");
            unsigned int enumSize = enumSizeStr ? atoi(enumSizeStr) : 0;
            if (enumSize != 32) {
                std::cerr << "addUserDataTypes: enum " << typeName
                          << " has unexpected size of " << enumSize
                          << " bits instead of 32"
                          << std::endl;
                continue;
            }

            userDataType.typeKind = UA_DATATYPEKIND_ENUM;
            userDataType.memSize = sizeof(UA_UInt32);
            userDataType.overlayable = UA_BINARY_OVERLAYABLE_INTEGER;

            std::cout << "\nenum " << typeName << " {"
                      <<  " # tns:" << userTypes.size()
                      << std::endl;
            for (xmlNode* choice = node->children; choice; choice = choice->next) {
                if (choice->type != XML_ELEMENT_NODE)
                    continue;
                if (strcmp(nodeName(choice), "Documentation") == 0) {
                    for (xmlNode* doc = choice->children; doc; doc = doc->next) {
                        if (doc->content)
                            std::cout << "# " << doc->content << std::endl;
                    }
                    continue;
                }
                if (strcmp(nodeName(choice), "EnumeratedValue") != 0) {
                    std::cerr << "# unexpected node " << nodeName(choice) << std::endl;
                    continue;
                }
                const char* choiceName = getProp(choice, "Name");
                const char* choiceValue = getProp(choice, "Value");
                std::cout << "  " << choiceName << " = " << choiceValue << std::endl;
                // TODO: Maybe do something with the enum strings?
            }
            std::cout << "};" << std::endl;
        }

        // Update current custom type wrapper array for searching type names
        if (userDataType.memSize) {
            userDataType.typeId =  UA_NODEID_STRING(nsIndex, const_cast<char*>(typeName));
            userDataType.typeName = strdup(typeName);
            userTypes.push_back(userDataType);
            // Someone made all UA_DataTypeArray members const. Thank you very much.
            *const_cast<size_t*>(&currentCustomTypes.typesSize) = userTypes.size();
            currentCustomTypes.types = userTypes.data();
        }
    }

    if (userTypes.empty())
        return;

    // Check for unresolved types (defined after use)
    std::cout << "### Looking up " << unresolvedTypes.size() << " unresolved types "<< std::endl;
    for (auto unresolved: unresolvedTypes) {
        UA_DataType &type = userTypes[unresolved.typeIndex];
        UA_DataTypeMember& member = type.members[unresolved.memberIndex];
        const UA_DataType *memberType = getTypeByName(unresolved.typeName, &currentCustomTypes);
        if (!memberType) {
            std::cerr << type.typeName << '.' << member.memberName
                      << "has unknown type" << unresolved.typeName <<std::endl;
            continue;
        }
#ifdef UA_DATATYPES_USE_POINTER
        member.memberType = memberType;
#else
        member.memberTypeIndex = memberType->typeIndex;
#endif
        std::cout << "#### " << type.typeName << '.' << member.memberName
                  << " = " << unresolved.typeName << " = " << getTypeByName(unresolved.typeName, &currentCustomTypes)
                  << " fixed to " << memberTypeOf(member)->typeName << std::endl;
    }
    // Move collected types to the beginning of the customTypes linked list.
    UA_DataTypeArray* newCustomTypes = static_cast<UA_DataTypeArray*>(malloc(sizeof(UA_DataTypeArray)));
    newCustomTypes->next = *customTypes;
    *const_cast<size_t*>(&newCustomTypes->typesSize) = userTypes.size();
    newCustomTypes->types = static_cast<UA_DataType*>(malloc(newCustomTypes->typesSize * sizeof(UA_DataType)));
    memcpy(const_cast<UA_DataType*>(newCustomTypes->types), userTypes.data(), newCustomTypes->typesSize * sizeof(UA_DataType));
    *customTypes = newCustomTypes;
#endif
}

void
clearTypeDictionaries(UA_Client* client) {
    UA_ClientConfig *config = UA_Client_getConfig(client);
    const UA_DataTypeArray *customTypes = config->customDataTypes;
    while (customTypes) {
        delete(customTypes->types);
        const UA_DataTypeArray *nextCustomTypes = customTypes->next;
        delete(customTypes);
        customTypes = nextCustomTypes;
    }
    config->customDataTypes = nullptr;
}

// retrieves all dictionaries of the OPC UA server
void
getTypeDictionaries(UA_Client* client) {
    clearTypeDictionaries(client);
    UA_ClientConfig *config = UA_Client_getConfig(client);
    UA_BrowseResponse bResp;
    UA_BrowseResponse_init(&bResp);
    browseNodeId(client, UA_NODEID_NUMERIC(0, UA_NS0ID_OPCBINARYSCHEMA_TYPESYSTEM), &bResp);
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
        for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
            UA_NodeId& nodeId = bResp.results[i].references[j].nodeId.nodeId;
            if (nodeId.namespaceIndex == 0) // user directories only
                continue;
            UA_Variant variant;
            UA_Variant_init(&variant);
            UA_Client_readValueAttribute(client, nodeId, &variant);
            size_t localIndex = j * UA_NAMESPACEOFFSET + nodeId.namespaceIndex;
            if (!UA_Variant_hasScalarType(&variant, &UA_TYPES[UA_TYPES_BYTESTRING])) {
                std::cerr << "getTypeDictionaries: "<< nodeId
                          << " has unexpected type " << (variant.type ? variant.type->typeName : "None" )
                          << (!variant.type || UA_Variant_isScalar(&variant) ? "" : "[]")
                          << std::endl;
            } else {
                UA_ByteString* bytestring = static_cast<UA_ByteString*>(variant.data);
                xmlDocPtr xmldoc = xmlReadMemory(reinterpret_cast<const char*>(bytestring->data),
                    static_cast<int>(bytestring->length), "include.xml", NULL, 0);
                if (!xmldoc) {
                    std::cerr << "getTypeDictionaries: error parsing " << nodeId << std::endl;
                } else {
                    std::cout << "getTypeDictionaries: nodeId " << nodeId << " localIndex " << localIndex << std::endl;
                    // xmlDocDump(stdout, xmldoc);
                    addUserDataTypes(xmlDocGetRootElement(xmldoc), nodeId.namespaceIndex, const_cast<UA_DataTypeArray**>(&config->customDataTypes));
                    xmlFreeDoc(xmldoc);
                }
            }
            UA_Variant_clear(&variant);
        }
    }
}

// callbacks

void
SessionOpen62541::connectionStatusChanged (
    UA_SecureChannelState newChannelState,
    UA_SessionState newSessionState,
    UA_StatusCode newConnectStatus)
{
    connectStatus = newConnectStatus;
    if (UA_STATUS_IS_BAD(newConnectStatus)) {
        if (debug)
            std::cout << "Session " << name
                      << " irrecoverably failed: "
                      << UA_StatusCode_name(connectStatus)
                      << std::endl;
        return;
    }

    if (newChannelState != channelState) {
        if (debug)
            std::cout << "Session " << name
                      << ": secure channel state changed from "
                      << channelState << " to "
                      << newChannelState
                      << std::endl;
// TODO: What to do for each channelState change?
        switch (newChannelState) {
            case UA_SECURECHANNELSTATE_CLOSED:
                // Deactivated by user or server shut down
                markConnectionLoss();
                registeredItemsNo = 0;
                break;
            case UA_SECURECHANNELSTATE_FRESH:
                if (sessionState == UA_SESSIONSTATE_CREATED) {
                    // The server has shut down
                    if (autoConnect)
                        autoConnector.start();
                }
                break;
            case UA_SECURECHANNELSTATE_OPEN: {
                // Connection to server has been established
                UA_ClientConfig *config = UA_Client_getConfig(client);
                std::string token;
                auto type = config->userIdentityToken.content.decoded.type;
                if (type == &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN])
                    token = " (username token)";
                if (type == &UA_TYPES[UA_TYPES_X509IDENTITYTOKEN])
                    token = " (certificate token)";
                std::cerr << "OPC UA session " << name
                        << ": connect succeeded as '" << securityUserName << "'" << token
                        << " with security level " << securityLevel
                        << " (mode=" << config->securityMode
                        << "; policy=" << securityPolicyString(config->securityPolicyUri)
                        << ")" << std::endl;
                if (config->securityMode == UA_MESSAGESECURITYMODE_NONE) {
                    errlogPrintf("OPC UA session %s: WARNING - this session uses *** NO SECURITY ***\n",
                                name.c_str());
                }
                break;
            }
            default: break;
        }
        channelState = newChannelState;
    }

    if (newSessionState != sessionState) {
        if (debug)
            std::cout << "Session " << name
                      << ": session state changed from "
                      << sessionState << " to "
                      << newSessionState
                      << std::endl;
// TODO: What to do for each sessionState change?
        switch (newSessionState) {
            case UA_SESSIONSTATE_ACTIVATED:
            {
                // read some settings from server
                UA_Variant value;
                UA_StatusCode status;
                unsigned int max;

                UA_Variant_init(&value);

                // max nodes per read request
                status = UA_Client_readValueAttribute(client,
                    UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERCAPABILITIES_OPERATIONLIMITS_MAXNODESPERREAD)
                    , &value);
                if (status == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_UINT32]))
                    MaxNodesPerRead = *static_cast<UA_UInt32*>(value.data);
                UA_Variant_clear(&value);
                if (MaxNodesPerRead > 0 && readNodesMax > 0)
                    max = std::min<unsigned int>(MaxNodesPerRead, readNodesMax);
                else
                    max = MaxNodesPerRead + readNodesMax;
                if (max != readNodesMax)
                    reader.setParams(max, readTimeoutMin, readTimeoutMax);

                // max nodes per write request
                status = UA_Client_readValueAttribute(client,
                    UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERCAPABILITIES_OPERATIONLIMITS_MAXNODESPERWRITE)
                    , &value);
                if (status == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_UINT32]))
                    MaxNodesPerWrite = *static_cast<UA_UInt32*>(value.data);
                UA_Variant_clear(&value);
                if (MaxNodesPerWrite > 0 && writeNodesMax > 0)
                    max = std::min<unsigned int>(MaxNodesPerWrite, writeNodesMax);
                else
                    max = MaxNodesPerWrite + writeNodesMax;
                if (max != writeNodesMax)
                    writer.setParams(max, writeTimeoutMin, writeTimeoutMax);

                // namespaces
                status = UA_Client_readValueAttribute(client,
                    UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_NAMESPACEARRAY)
                    , &value);
                if (status == UA_STATUSCODE_GOOD && UA_Variant_hasArrayType(&value, &UA_TYPES[UA_TYPES_STRING]))
                    updateNamespaceMap(static_cast<UA_String*>(value.data), static_cast<UA_UInt16>(value.arrayLength));
                UA_Variant_clear(&value);

                getTypeDictionaries(client);

                rebuildNodeIds();
                registerNodes();
                createAllSubscriptions();
                addAllMonitoredItems();
                if (debug) {
                    std::cout << "Session " << name
                              << ": triggering initial read for all "
                              << items.size() << " items"
                              << std::endl;
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
                reader.pushRequest(cargo, menuPriorityHIGH);
                break;
            }
            default: break;
        }
        sessionState = newSessionState;
    }
}

void
SessionOpen62541::readComplete (UA_UInt32 transactionId,
                            UA_ReadResponse* response)
{
    Guard G(opslock);
    auto it = outstandingOps.find(transactionId);
    if (it == outstandingOps.end()) {
        errlogPrintf("OPC UA session %s: (readComplete) received a callback "
                     "with unknown transaction id %u - ignored\n",
                     name.c_str(), transactionId);
    } else if (!UA_STATUS_IS_BAD(response->responseHeader.serviceResult)) {
        if (debug >= 2)
            std::cout << "Session " << name
                      << ": (readComplete) getting data for read service"
                      << " (transaction id " << transactionId
                      << "; data for " << response->resultsSize << " items)"
                      << std::endl;
        if ((*it->second).size() != response->resultsSize)
            errlogPrintf("OPC UA session %s: (readComplete) received a callback "
                         "with %zu values for a request containing %zu items\n",
                         name.c_str(), response->resultsSize, (*it->second).size());
        UA_UInt32 i = 0;
        for (auto item : (*it->second)) {
            if (i >= response->resultsSize) {
                item->setIncomingEvent(ProcessReason::readFailure);
            } else {
                if (debug >= 5) {
                    std::cout << "** Session " << name
                              << ": (readComplete) getting data for item "
                              << item
                              << " = " << response->results[i].value
                              << ' ' << UA_StatusCode_name(response->results[i].status)
                              << std::endl;
                }
                ProcessReason reason = ProcessReason::readComplete;
                if (UA_STATUS_IS_BAD(response->results[i].status))
                    reason = ProcessReason::readFailure;
                if (response->results[i].value.type &&
                    response->results[i].value.type->typeKind == UA_TYPES_EXTENSIONOBJECT) {
                    UA_ExtensionObject &extensionObject = *reinterpret_cast<UA_ExtensionObject *>(response->results[i].value.data);
                    std::cout << "Item " << item << " UA_ExtensionObject encoding: " << extensionObject.encoding << std::endl;
                    if (extensionObject.encoding == UA_EXTENSIONOBJECT_ENCODED_BYTESTRING) {
                        UA_ClientConfig *config = UA_Client_getConfig(client);
                        UA_NodeId dataTypeId;
                        UA_NodeId_init(&dataTypeId);
                        UA_Client_readDataTypeAttribute(client, item->getNodeId(), &dataTypeId);
                        UA_QualifiedName browseName;
                        UA_QualifiedName_init(&browseName);
                        UA_Client_readBrowseNameAttribute(client, dataTypeId, &browseName);
                        std::cout << "Item " << item
                                  << " encoded extension object, typeId " << extensionObject.content.encoded.typeId
                                  << " , dataTypeId " << dataTypeId
                                  << " browseName " << browseName
                                  << std::endl;
                        std::string typeName;
                        typeName += browseName.name;
                        const UA_DataType *userDataType = getTypeByName(typeName.c_str(), config->customDataTypes);
                        if (!userDataType) {
                            std::cout << "Item " << item
                                      << " cannot find data type " << typeName
                                      << std::endl;
                        } else {
                            UA_NodeId_copy(&dataTypeId, &const_cast<UA_DataType *>(userDataType)->binaryEncodingId);
                            std::cout << "Session " << name
                                      << ": re-triggering initial read for " << item
                                      << std::endl;
                            auto cargo = std::vector<std::shared_ptr<ReadRequest>>(1);
                            item->setState(ConnectionStatus::initialRead);
                            cargo[0] = std::make_shared<ReadRequest>();
                            cargo[0]->item = item;
                            //reader.pushRequest(cargo, menuPriorityHIGH);
                        }
                        UA_QualifiedName_clear(&browseName);
                        UA_NodeId_clear(&dataTypeId);
                        continue;
                    }
                }
                std::cout << "Item " << item
                          << "->setIncomingData()"
                          << std::endl;
                item->setIncomingData(response->results[i], reason);
            }
            i++;
        }
        outstandingOps.erase(it);
    } else {
        if (debug)
            std::cout << "Session " << name
                      << ": (readComplete) for read service"
                      << " (transaction id " << transactionId
                      << ") failed with status "
                      << UA_StatusCode_name(response->responseHeader.serviceResult)
                      << std::endl;
        for (auto item : (*it->second)) {
            if (debug >= 5) {
                std::cout << "** Session " << name
                          << ": (readComplete) filing read error (no data) for item "
                          << item << std::endl;
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
                            UA_WriteResponse* response)
{
    Guard G(opslock);
    auto it = outstandingOps.find(transactionId);
    if (it == outstandingOps.end()) {
        errlogPrintf("OPC UA session %s: (writeComplete) received a callback "
                     "with unknown transaction id %u - ignored\n",
                     name.c_str(), transactionId);
    } else if (!UA_STATUS_IS_BAD(response->responseHeader.serviceResult)) {
        if (debug >= 2)
            std::cout << "Session " << name
                      << ": (writeComplete) getting results for write service"
                      << " (transaction id " << transactionId
                      << "; results for " << response->resultsSize << " items)" << std::endl;
        UA_UInt32 i = 0;
        for (auto item : (*it->second)) {
            if (debug >= 5) {
                std::cout << "** Session " << name
                          << ": (writeComplete) getting results for item "
                          << item
                          << ' ' << UA_StatusCode_name(response->results[i])
                          << std::endl;
            }
            ProcessReason reason = ProcessReason::writeComplete;
            if (UA_STATUS_IS_BAD(response->results[i]))
                reason = ProcessReason::writeFailure;
            item->setIncomingEvent(reason);
            item->setState(ConnectionStatus::up);
            i++;
        }
        outstandingOps.erase(it);
    } else {
        if (debug)
            std::cout << "Session " << name
                      << ": (writeComplete) for write service"
                      << " (transaction id " << transactionId
                      << ") failed with status "
                      << UA_StatusCode_name(response->responseHeader.serviceResult)
                      << std::endl;
        for (auto item : (*it->second)) {
            if (debug >= 5) {
                std::cout << "** Session " << name
                          << ": (writeComplete) filing write error for item "
                          << item
                          << std::endl;
            }
            item->setIncomingEvent(ProcessReason::writeFailure);
            item->setState(ConnectionStatus::up);
        }
        outstandingOps.erase(it);
    }
}

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
              << items << " items"
              << std::endl;
    if (level >= 1) {
        for (auto &it : sessions) {
            it.second->show(level-1);
        }
    }
}

SessionOpen62541::~SessionOpen62541 ()
{
    if (client)
        disconnect(); // also deletes client
}

void
SessionOpen62541::initHook (initHookState state)
{
    switch (state) {
    case initHookAfterIocRunning:
    {
        errlogPrintf("OPC UA: Autoconnecting sessions\n");
        for (auto &it : sessions) {
            it.second->markConnectionLoss();
            if (it.second->autoConnect)
                it.second->connect();
        }
        epicsThreadOnce(&DevOpcua::session_open62541_atexit_once, &DevOpcua::session_open62541_atexit_register, nullptr);
        break;
    }
    default:
        break;
    }
}

void
SessionOpen62541::atExit (void *)
{
    errlogPrintf("OPC UA: Disconnecting sessions\n");
    for (auto &it : sessions) {
        it.second->disconnect();
        SessionOpen62541 *session = it.second;
        if (session->isConnected())
            session->disconnect();
    }
}

} // namespace DevOpcua
