/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
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
    , dataElement(std::unique_ptr<DataElementUaSdk>(new DataElementUaSdk(this)))
{
    if (info.identifierIsNumeric) {
        nodeid = std::unique_ptr<UaNodeId>(new UaNodeId(info.identifierNumber, info.namespaceIndex));
    } else {
        nodeid = std::unique_ptr<UaNodeId>(new UaNodeId(info.identifierString.c_str(), info.namespaceIndex));
    }

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
ItemUaSdk::show (int level) const
{
    std::cout << "item"
              << " ns="     << linkinfo.namespaceIndex;
    if (linkinfo.identifierIsNumeric)
        std::cout << ";i=" << linkinfo.identifierNumber;
    else
        std::cout << ";s=" << linkinfo.identifierString;
    std::cout << " context=" << linkinfo.subscription
              << "@" << session->getName()
              << " sampling=" << linkinfo.samplingInterval
              << " qsize=" << linkinfo.queueSize
              << " discard=" << (linkinfo.discardOldest ? "old" : "new")
              << " timestamp=" << (linkinfo.useServerTimestamp ? "server" : "source")
              << " output=" << (linkinfo.isOutput ? "y" : "n")
              << " monitor=" << (linkinfo.monitor ? "y" : "n")
              << std::endl;
}

} // namespace DevOpcua
