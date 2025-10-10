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

#include "ItemUaSdk.h"

#include <iostream>
#include <memory>

#include <opcua_statuscodes.h>
#include <uaclientsdk.h>
#include <uanodeid.h>

#include "DataElementUaSdk.h"
#include "RecordConnector.h"
#include "SessionUaSdk.h"
#include "SubscriptionUaSdk.h"
#include "devOpcua.h"
#include "opcuaItemRecord.h"

namespace DevOpcua
{

using namespace UaClientSdk;

/* Specific implementation of Item's factory method */
Item *
Item::newItem (const linkInfo &info)
{
    return new ItemUaSdk(info);
}

ItemUaSdk::ItemUaSdk (const linkInfo &info)
    : Item(info)
    , subscription(nullptr)
    , session(nullptr)
    , registered(false)
    , revisedSamplingInterval(0.0)
    , revisedQueueSize(0)
    , dataTreeDirty(false)
    , lastStatus(OpcUa_BadServerNotConnected)
    , lastReason(ProcessReason::connectionLoss)
{
    if (linkinfo.subscription != "" && linkinfo.monitor) {
        subscription = SubscriptionUaSdk::find(linkinfo.subscription);
        subscription->addItemUaSdk(this);
        session = &subscription->getSessionUaSdk();
    } else {
        session = SessionUaSdk::find(linkinfo.session);
    }
    session->addItemUaSdk(this);
}

ItemUaSdk::~ItemUaSdk ()
{
    subscription->removeItemUaSdk(this);
    session->removeItemUaSdk(this);
}

void
ItemUaSdk::rebuildNodeId ()
{
    OpcUa_UInt16 ns = session->mapNamespaceIndex(linkinfo.namespaceIndex);
    if (linkinfo.identifierIsNumeric) {
        nodeid = std::unique_ptr<UaNodeId>(new UaNodeId(linkinfo.identifierNumber, ns));
    } else {
        nodeid = std::unique_ptr<UaNodeId>(new UaNodeId(linkinfo.identifierString.c_str(), ns));
    }
    registered = false;
}

void
ItemUaSdk::requestWriteIfDirty ()
{
    Guard G(dataTreeWriteLock);
    if (dataTreeDirty)
        recConnector->requestRecordProcessing(ProcessReason::writeRequest);
}

void
ItemUaSdk::show (int level) const
{
    std::cout << "item"
              << " ns=";
    if (nodeid && (nodeid->namespaceIndex() != linkinfo.namespaceIndex))
        std::cout << nodeid->namespaceIndex() << "(" << linkinfo.namespaceIndex << ")";
    else
        std::cout << linkinfo.namespaceIndex;
    if (linkinfo.identifierIsNumeric)
        std::cout << ";i=" << linkinfo.identifierNumber;
    else
        std::cout << ";s=" << linkinfo.identifierString;
    std::cout << " record=" << recConnector->getRecordName()
              << " state=" << connectionStatusString(recConnector->state())
              << " status=" << UaStatus(lastStatus).toString().toUtf8() << " dataDirty=" << (dataTreeDirty ? "y" : "n")
              << " context=" << linkinfo.subscription << "@" << session->getName()
              << " sampling=" << revisedSamplingInterval << "(" << linkinfo.samplingInterval << ")"
              << " deadband=" << linkinfo.deadband << " qsize=" << revisedQueueSize << "(" << linkinfo.queueSize << ")"
              << " cqsize=" << linkinfo.clientQueueSize << " discard=" << (linkinfo.discardOldest ? "old" : "new")
              << " timestamp=" << linkOptionTimestampString(linkinfo.timestamp);
    if (linkinfo.timestamp == LinkOptionTimestamp::data)
        std::cout << "@" << linkinfo.timestampElement;
    std::cout << " bini=" << linkOptionBiniString(linkinfo.bini) << " output=" << (linkinfo.isOutput ? "y" : "n")
              << " monitor=" << (linkinfo.monitor ? "y" : "n")
              << " registered=" << (registered ? nodeid->toString().toUtf8() : "-") << "("
              << (linkinfo.registerNode ? "y" : "n") << ")" << std::endl;

    if (level >= 1) {
        if (auto re = dataTree.root().lock()) {
            re->show(level, 1);
        }
        std::cout.flush();
    }
}

int
ItemUaSdk::debug () const
{
    return recConnector->debug();
}

void
ItemUaSdk::copyAndClearOutgoingData (_OpcUa_WriteValue &wvalue)
{
    Guard G(dataTreeWriteLock);
    if (auto pd = dataTree.root().lock()) {
        pd->getOutgoingData().copyTo(&wvalue.Value.Value);
        pd->clearOutgoingData();
    }
    dataTreeDirty = false;
}

epicsTime
ItemUaSdk::uaToEpicsTime (const UaDateTime &dt, const OpcUa_UInt16 pico10)
{
    epicsTimeStamp ts;
    ts.secPastEpoch = static_cast<epicsUInt32>(dt.toTime_t()) - POSIX_TIME_AT_EPICS_EPOCH;
    ts.nsec = static_cast<epicsUInt32>(static_cast<OpcUa_Int64>(dt) % UA_SECS_TO_100NS) * 100 + pico10 / 100;
    return epicsTime(ts);
}

void
ItemUaSdk::setIncomingData (const OpcUa_DataValue &value, ProcessReason reason, const UaNodeId *typeId)
{
    tsClient = epicsTime::getCurrent();
    if (OpcUa_IsNotBad(value.StatusCode)) {
        tsSource = uaToEpicsTime(UaDateTime(value.SourceTimestamp), value.SourcePicoseconds);
        tsServer = uaToEpicsTime(UaDateTime(value.ServerTimestamp), value.ServerPicoseconds);
    } else {
        tsSource = tsClient;
        tsServer = tsClient;
        tsData = tsClient;
    }
    setReason(reason);
    if (getLastStatus() == OpcUa_BadServerNotConnected && value.StatusCode == OpcUa_BadNodeIdUnknown)
        errlogPrintf("OPC UA session %s: item ns=%d;%s%.*d%s : BadNodeIdUnknown\n",
                     session->getName().c_str(),
                     linkinfo.namespaceIndex,
                     (linkinfo.identifierIsNumeric ? "i=" : "s="),
                     (linkinfo.identifierIsNumeric ? 1 : 0),
                     (linkinfo.identifierIsNumeric ? linkinfo.identifierNumber : 0),
                     (linkinfo.identifierIsNumeric ? "" : linkinfo.identifierString.c_str()));

    setLastStatus(value.StatusCode);

    if (auto pd = dataTree.root().lock()) {
        const std::string *timefrom = nullptr;
        if (linkinfo.timestamp == LinkOptionTimestamp::data && linkinfo.timestampElement.length())
            timefrom = &linkinfo.timestampElement;
        pd->setIncomingData(value.Value, reason, timefrom, typeId);
    }

    if (linkinfo.isItemRecord) {
        if (recConnector->state() == ConnectionStatus::initialRead && reason == ProcessReason::readComplete
            && recConnector->bini() == LinkOptionBini::write) {
            recConnector->setState(ConnectionStatus::initialWrite);
            recConnector->requestRecordProcessing(ProcessReason::writeRequest);
        } else {
            recConnector->requestRecordProcessing(reason);
        }
    }
}

void
ItemUaSdk::setIncomingEvent (const ProcessReason reason)
{
    tsClient = epicsTime::getCurrent();
    setReason(reason);
    if (!(reason == ProcessReason::incomingData || reason == ProcessReason::readComplete)) {
        tsSource = tsClient;
        tsServer = tsClient;
        tsData = tsClient;
        if (reason == ProcessReason::connectionLoss)
            setLastStatus(OpcUa_BadServerNotConnected);
    }

    if (auto pd = dataTree.root().lock()) {
        pd->setIncomingEvent(reason);
    }

    if (linkinfo.isItemRecord)
        recConnector->requestRecordProcessing(reason);
}

void
ItemUaSdk::setState (const ConnectionStatus state)
{
    if (auto pd = dataTree.root().lock()) {
        pd->setState(state);
    }
    if (linkinfo.isItemRecord)
        recConnector->setState(state);
}

void
ItemUaSdk::markAsDirty ()
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
ItemUaSdk::getStatus (epicsUInt32 *code, char *text, const epicsUInt32 len, epicsTimeStamp *ts)
{
    *code = lastStatus.code();
    if (text && len) {
        strncpy(text, lastStatus.toString().toUtf8(), len);
        text[len - 1] = '\0';
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
