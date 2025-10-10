/*************************************************************************\
* Copyright (c) 2018-2023 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#define epicsExportSharedSymbols
#include "ItemOpen62541.h"
#include "RecordConnector.h"
#include "SubscriptionOpen62541.h"
#include "SessionOpen62541.h"
#include "DataElementOpen62541.h"

#include <errlog.h>
#include <epicsTypes.h>
#include <epicsTime.h>

#include <open62541/client.h>

#include <string>
#include <iostream>

namespace DevOpcua {

/* Specific implementation of Item's factory method */
Item *
Item::newItem(const linkInfo &info)
{
    return new ItemOpen62541(info);
}

ItemOpen62541::ItemOpen62541(const linkInfo &info)
    : Item(info)
    , subscription(nullptr)
    , session(nullptr)
    , registered(false)
    , revisedSamplingInterval(0.0)
    , revisedQueueSize(0)
    , dataTreeDirty(false)
    , lastStatus(UA_STATUSCODE_BADSERVERNOTCONNECTED)
    , lastReason(ProcessReason::connectionLoss)
{
    UA_NodeId_init(&nodeId);
    if (linkinfo.subscription != "" && linkinfo.monitor) {
        subscription = SubscriptionOpen62541::find(linkinfo.subscription);
        subscription->addItemOpen62541(this);
        session = &subscription->getSessionOpen62541();
    } else {
        session = SessionOpen62541::find(linkinfo.session);
    }
    session->addItemOpen62541(this);
}

ItemOpen62541::~ItemOpen62541 ()
{
    subscription->removeItemOpen62541(this);
    session->removeItemOpen62541(this);
    UA_NodeId_clear(&nodeId);
}

void
ItemOpen62541::rebuildNodeId ()
{
    UA_UInt16 ns = session->mapNamespaceIndex(linkinfo.namespaceIndex);
    UA_NodeId_clear(&nodeId);
    if (linkinfo.identifierIsNumeric) {
        nodeId = UA_NODEID_NUMERIC(ns, linkinfo.identifierNumber);
    } else {
        nodeId = UA_NODEID_STRING_ALLOC(ns, linkinfo.identifierString.c_str());
    }
    registered = false;
}

void
ItemOpen62541::requestWriteIfDirty()
{
    Guard G(dataTreeWriteLock);
    if (dataTreeDirty)
        recConnector->requestRecordProcessing(ProcessReason::writeRequest);
}

void
ItemOpen62541::show (int level) const
{
    std::cout << "item"
              << " ns=";
    if (nodeId.namespaceIndex != linkinfo.namespaceIndex)
        std::cout << nodeId.namespaceIndex << "(" << linkinfo.namespaceIndex << ")";
    else
        std::cout << linkinfo.namespaceIndex;
    if (linkinfo.identifierIsNumeric)
        std::cout << ";i=" << linkinfo.identifierNumber;
    else
        std::cout << ";s=" << linkinfo.identifierString;
    std::cout << " record=" << recConnector->getRecordName()
              << " state=" << connectionStatusString(recConnector->state())
              << " status=" << UA_StatusCode_name(lastStatus)
              << " dataDirty=" << (dataTreeDirty ? "y" : "n")
              << " context=" << linkinfo.subscription << "@" << session->getName()
              << " sampling=" << revisedSamplingInterval << "(" << linkinfo.samplingInterval << ")"
              << " deadband=" << linkinfo.deadband
              << " qsize=" << revisedQueueSize << "(" << linkinfo.queueSize << ")"
              << " cqsize=" << linkinfo.clientQueueSize
              << " discard=" << (linkinfo.discardOldest ? "old" : "new")
              << " timestamp=" << linkOptionTimestampString(linkinfo.timestamp);
    if (linkinfo.timestamp == LinkOptionTimestamp::data)
        std::cout << "@" << linkinfo.timestampElement;
    std::cout << " bini=" << linkOptionBiniString(linkinfo.bini)
              << " output=" << (linkinfo.isOutput ? "y" : "n")
              << " monitor=" << (linkinfo.monitor ? "y" : "n")
              << " registered=";
    if (registered)
        std::cout << nodeId;
        else std::cout << "-";
    std::cout << "(" << (linkinfo.registerNode ? "y" : "n") << ")"
              << std::endl;

    if (level >= 1) {
        if (auto re = dataTree.root().lock()) {
            re->show(level, 1);
        }
        std::cout.flush();
    }
}

int ItemOpen62541::debug() const
{
    return recConnector->debug();
}

void
ItemOpen62541::copyAndClearOutgoingData(UA_WriteValue &wvalue)
{
    Guard G(dataTreeWriteLock);
    if (auto pd = dataTree.root().lock()) {
        UA_Variant_copy(&pd->getOutgoingData(), &wvalue.value.value);
        pd->clearOutgoingData();
    }
    dataTreeDirty = false;
}

epicsTime
ItemOpen62541::uaToEpicsTime (const UA_DateTime &dt, const UA_UInt16 pico10)
{
    epicsTimeStamp ts;
    ts.secPastEpoch = static_cast<epicsUInt32>(UA_DateTime_toUnixTime(dt)) - POSIX_TIME_AT_EPICS_EPOCH;
    ts.nsec         = static_cast<epicsUInt32>(dt%UA_DATETIME_SEC) * (1000000000LL/UA_DATETIME_SEC) + pico10 / 100;
    return epicsTime(ts);
}

void
ItemOpen62541::setIncomingData(UA_DataValue &value, ProcessReason reason)
{
    tsClient = epicsTime::getCurrent();
    if (!UA_STATUS_IS_BAD(value.status)) {
        tsSource = uaToEpicsTime(value.sourceTimestamp, value.sourcePicoseconds);
        tsServer = uaToEpicsTime(value.serverTimestamp, value.serverPicoseconds);
    } else {
        tsSource = tsClient;
        tsServer = tsClient;
        tsData = tsClient;
    }
    setReason(reason);
    if (getLastStatus() == UA_STATUSCODE_BADSERVERNOTCONNECTED && value.status == UA_STATUSCODE_BADNODEIDUNKNOWN)
        errlogPrintf("OPC UA session %s: item ns=%d;%s%.*d%s : BadNodeIdUnknown\n",
                     session->getName().c_str(),
                     linkinfo.namespaceIndex,
                     (linkinfo.identifierIsNumeric ? "i=" : "s="),
                     (linkinfo.identifierIsNumeric ? 1 : 0),
                     (linkinfo.identifierIsNumeric ? linkinfo.identifierNumber : 0),
                     (linkinfo.identifierIsNumeric ? "" : linkinfo.identifierString.c_str()));

    setLastStatus(value.status);

    if (auto pd = dataTree.root().lock()) {
        const std::string *timefrom = nullptr;
        if (linkinfo.timestamp == LinkOptionTimestamp::data && linkinfo.timestampElement.length())
            timefrom = &linkinfo.timestampElement;
        pd->setIncomingData(value.value, reason, timefrom);
        value.value.storageType = UA_VARIANT_DATA_NODELETE; // take ownership of data
    }

    if (linkinfo.isItemRecord) {
        if (recConnector->state() == ConnectionStatus::initialRead
                && reason == ProcessReason::readComplete
                && recConnector->bini() == LinkOptionBini::write) {
            recConnector->setState(ConnectionStatus::initialWrite);
            recConnector->requestRecordProcessing(ProcessReason::writeRequest);
        } else {
            recConnector->requestRecordProcessing(reason);
        }
    }
}

void
ItemOpen62541::setIncomingEvent(const ProcessReason reason)
{
    tsClient = epicsTime::getCurrent();
    setReason(reason);
    if (!(reason == ProcessReason::incomingData || reason == ProcessReason::readComplete)) {
        tsSource = tsClient;
        tsServer = tsClient;
        tsData = tsClient;
        if (reason == ProcessReason::connectionLoss)
            setLastStatus(UA_STATUSCODE_BADSERVERNOTCONNECTED);
    }

    if (auto pd = dataTree.root().lock()) {
        pd->setIncomingEvent(reason);
    }

    if (linkinfo.isItemRecord)
        recConnector->requestRecordProcessing(reason);
}

void
ItemOpen62541::setState(const ConnectionStatus state)
{
    if (auto pd = dataTree.root().lock()) {
        pd->setState(state);
    }
    if (linkinfo.isItemRecord)
        recConnector->setState(state);
}

void
ItemOpen62541::markAsDirty()
{
    if (recConnector->plinkinfo->isItemRecord) {
        Guard G(dataTreeWriteLock);
        if (!dataTreeDirty) {
            dataTreeDirty = true;
            if (recConnector->woc() == menuWocIMMEDIATE)
                recConnector->requestRecordProcessing(ProcessReason::writeRequest);
        }
    }
}

void
ItemOpen62541::getStatus(epicsUInt32 *code, char *text, const epicsUInt32 len, epicsTimeStamp *ts)
{
    *code = lastStatus;
    if (text && len) {
        strncpy(text, UA_StatusCode_name(lastStatus), len);
        text[len-1] = '\0';
    }

    if (ts && recConnector) {
        switch (recConnector->plinkinfo->timestamp) {
        case LinkOptionTimestamp::server:
            *ts = tsServer;
            break;
        case LinkOptionTimestamp::source:
            *ts = tsSource;
            break;
        case LinkOptionTimestamp::data:
            *ts = tsData;
            break;
        }
    }
}

} // namespace DevOpcua
