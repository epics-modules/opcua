/*************************************************************************\
* Copyright (c) 2018-2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#include <memory>
#include <cstring>

#include "RecordConnector.h"
#include "opcuaItemRecord.h"
#include "ItemOpen62541.h"
#include "SubscriptionOpen62541.h"
#include "SessionOpen62541.h"
#include "DataElementOpen62541.h"

namespace DevOpcua {

/* Specific implementation of Item's factory method */
Item *
Item::newItem(const linkInfo &info)
{
    return static_cast<Item*>(new ItemOpen62541(info));
}

ItemOpen62541::ItemOpen62541 (const linkInfo &info)
    : Item(info)
    , subscription(nullptr)
    , session(nullptr)
    , registered(false)
    , revisedSamplingInterval(0.0)
    , revisedQueueSize(0)
    , lastStatus(UA_STATUSCODE_BADSERVERNOTCONNECTED)
    , lastReason(ProcessReason::connectionLoss)
    , connState(ConnectionStatus::down)
{
    if (linkinfo.subscription != "" && linkinfo.monitor) {
        subscription = &SubscriptionOpen62541::findSubscription(linkinfo.subscription);
        subscription->addItemOpen62541(this);
        session = &subscription->getSessionOpen62541();
    } else {
        session = &SessionOpen62541::findSession(linkinfo.session);
    }
    session->addItemOpen62541(this);
}

ItemOpen62541::~ItemOpen62541 ()
{
    subscription->removeItemOpen62541(this);
    session->removeItemOpen62541(this);
}

void
ItemOpen62541::rebuildNodeId ()
{
/*
    UA_UInt16 ns = session->mapNamespaceIndex(linkinfo.namespaceIndex);
    if (linkinfo.identifierIsNumeric) {
        nodeid = std::unique_ptr<UaNodeId>(new UaNodeId(linkinfo.identifierNumber, ns));
    } else {
        nodeid = std::unique_ptr<UaNodeId>(new UaNodeId(linkinfo.identifierString.c_str(), ns));
    }
*/
    registered = false;
}

void
ItemOpen62541::show (int level) const
{
    std::cout << "item"
              << " ns=";
//    if (nodeid && (nodeid->namespaceIndex() != linkinfo.namespaceIndex))
//        std::cout << nodeid->namespaceIndex() << "(" << linkinfo.namespaceIndex << ")";
//    else
        std::cout << linkinfo.namespaceIndex;
    if (linkinfo.identifierIsNumeric)
        std::cout << ";i=" << linkinfo.identifierNumber;
    else
        std::cout << ";s=" << linkinfo.identifierString;
    std::cout << " record=" << recConnector->getRecordName()
              << " state=" << connectionStatusString(connState)
              << " status=" << UA_StatusCode_name(lastStatus)
              << " context=" << linkinfo.subscription
              << "@" << session->getName()
              << " sampling=" << revisedSamplingInterval
              << "(" << linkinfo.samplingInterval << ")"
              << " qsize=" << revisedQueueSize
              << "(" << linkinfo.queueSize << ")"
              << " cqsize=" << linkinfo.clientQueueSize
              << " discard=" << (linkinfo.discardOldest ? "old" : "new")
              << " timestamp=" << (linkinfo.useServerTimestamp ? "server" : "source")
              << " bini=" << linkOptionBiniString(linkinfo.bini)
              << " output=" << (linkinfo.isOutput ? "y" : "n")
              << " monitor=" << (linkinfo.monitor ? "y" : "n")
//              << " registered=" << (registered ? nodeid->toString().toUtf8() : "-" )
              << "(" << (linkinfo.registerNode ? "y" : "n") << ")"
              << std::endl;

    if (level >= 1) {
        if (auto re = rootElement.lock()) {
            re->show(level, 1);
        }
        std::cout.flush();
    }
}

int ItemOpen62541::debug() const
{
    return recConnector->debug();
}

//FIXME: in case of itemRecord there might be no rootElement
const UA_Variant &
ItemOpen62541::getOutgoingData() const
{
    if (auto pd = rootElement.lock()) {
        return pd->getOutgoingData();
    } else {
        throw std::runtime_error(SB() << "stale pointer to root data element");
    }
}

void
ItemOpen62541::clearOutgoingData()
{
    if (auto pd = rootElement.lock()) {
        pd->clearOutgoingData();
    }
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
ItemOpen62541::setIncomingData(const UA_DataValue &value, ProcessReason reason)
{
    tsClient = epicsTime::getCurrent();
    if (!UA_STATUS_IS_BAD(value.status)) {
        tsSource = uaToEpicsTime(value.sourceTimestamp, value.sourcePicoseconds);
        tsServer = uaToEpicsTime(value.serverTimestamp, value.serverPicoseconds);
    } else {
        tsSource = tsClient;
        tsServer = tsClient;
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

    if (auto pd = rootElement.lock())
        pd->setIncomingData(value.value, reason);

    if (linkinfo.isItemRecord) {
        if (state() == ConnectionStatus::initialRead
                && reason == ProcessReason::readComplete
                && recConnector->bini() == LinkOptionBini::write) {
            setState(ConnectionStatus::initialWrite);
            recConnector->requestRecordProcessing(ProcessReason::writeRequest);
        }
    }
}

void
ItemOpen62541::setIncomingEvent(const ProcessReason reason)
{
    tsClient = epicsTime::getCurrent();
    setReason(reason);
    if (reason == ProcessReason::connectionLoss) {
        tsSource = tsClient;
        tsServer = tsClient;
        setLastStatus(UA_STATUSCODE_BADSERVERNOTCONNECTED);
    }

    if (auto pd = rootElement.lock()) {
        pd->setIncomingEvent(reason);
    }

    if (linkinfo.isItemRecord)
        recConnector->requestRecordProcessing(reason);
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
        if (recConnector->plinkinfo->useServerTimestamp)
            *ts = tsServer;
        else
            *ts = tsSource;
    }
}

} // namespace DevOpcua
