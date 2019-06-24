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

#include <memory>

#include <uaclientsdk.h>
#include <uanodeid.h>

#include "RecordConnector.h"
#include "opcuaItemRecord.h"
#include "ItemUaSdk.h"
#include "SubscriptionUaSdk.h"
#include "SessionUaSdk.h"
#include "DataElementUaSdk.h"

namespace DevOpcua {

using namespace UaClientSdk;

ItemUaSdk::ItemUaSdk (const linkInfo &info)
    : Item(info)
    , subscription(nullptr)
    , session(nullptr)
    , registered(false)
{
    rebuildNodeId();

    if (linkinfo.subscription != "") {
        subscription = &SubscriptionUaSdk::findSubscription(linkinfo.subscription);
        subscription->addItemUaSdk(this);
        session = &subscription->getSessionUaSdk();
    } else {
        session = &SessionUaSdk::findSession(linkinfo.session);
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
    if (linkinfo.identifierIsNumeric) {
        nodeid = std::unique_ptr<UaNodeId>(new UaNodeId(linkinfo.identifierNumber, linkinfo.namespaceIndex));
    } else {
        nodeid = std::unique_ptr<UaNodeId>(new UaNodeId(linkinfo.identifierString.c_str(), linkinfo.namespaceIndex));
    }
    registered = false;
}

void
ItemUaSdk::show (int level) const
{
    std::cout << "item"
              << " ns="     << linkinfo.namespaceIndex;
    if (linkinfo.identifierIsNumeric)
        std::cout << ";i=" << linkinfo.identifierNumber;
    else
        std::cout << ";s=" << linkinfo.identifierString;
    if (linkinfo.isItemRecord)
        std::cout << " record=" << itemRecord->name;
    std::cout << " context=" << linkinfo.subscription
              << "@" << session->getName()
              << " sampling=" << linkinfo.samplingInterval
              << " qsize=" << linkinfo.queueSize
              << " discard=" << (linkinfo.discardOldest ? "old" : "new")
              << " timestamp=" << (linkinfo.useServerTimestamp ? "server" : "source")
              << " output=" << (linkinfo.isOutput ? "y" : "n")
              << " monitor=" << (linkinfo.monitor ? "y" : "n")
              << " registered=" << (registered ? nodeid->toString().toUtf8() : "-" )
              << "(" << (linkinfo.registerNode ? "y" : "n") << ")"
              << std::endl;

    if (level >= 1) {
        if (auto re = rootElement.lock()) {
            re->show(level, 1);
        }
        std::cout.flush();
    }
}

int ItemUaSdk::debug() const
{
    if (linkinfo.isItemRecord)
        return itemRecord->tpro;
    else if (auto pd = rootElement.lock())
        return pd->debug();
    else
        return 0;
}

void
ItemUaSdk::requestRecordProcessing (const ProcessReason reason) const
{
    if (auto pd = rootElement.lock()) {
        pd->requestRecordProcessing(reason);
    }
}

const UaVariant &
ItemUaSdk::getOutgoingData() const
{
    if (auto pd = rootElement.lock()) {
        return pd->getOutgoingData();
    } else {
        throw std::runtime_error(SB() << "stale pointer to root data element");
    }
}

void
ItemUaSdk::clearOutgoingData()
{
    if (auto pd = rootElement.lock()) {
        pd->clearOutgoingData();
    }
}

epicsTime
ItemUaSdk::uaToEpicsTime (const UaDateTime &dt, const OpcUa_UInt16 pico10)
{
    epicsTimeStamp ts;
    ts.secPastEpoch = static_cast<epicsUInt32>(dt.toTime_t()) - POSIX_TIME_AT_EPICS_EPOCH;
    ts.nsec         = static_cast<epicsUInt32>(dt.msec()) * 1000000 + pico10 / 100;
    return epicsTime(ts);
}

void
ItemUaSdk::setIncomingData(const OpcUa_DataValue &value)
{
    tsSource = uaToEpicsTime(UaDateTime(value.SourceTimestamp), value.SourcePicoseconds);
    tsServer = uaToEpicsTime(UaDateTime(value.ServerTimestamp), value.ServerPicoseconds);

    readStatus = value.StatusCode;

    if (auto pd = rootElement.lock()) {
        return pd->setIncomingData(value.Value);
    } else {
        throw std::runtime_error(SB() << "stale pointer to root data element");
    }
}

} // namespace DevOpcua
