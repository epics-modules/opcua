/*************************************************************************\
* Copyright (c) 2018-2021 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#define epicsExportSharedSymbols

#include <iostream>
#include <string>
#include <map>
#include <open62541/client_subscriptions.h>
#include <errlog.h>
#include "SubscriptionOpen62541.h"
#include "ItemOpen62541.h"
#include "DataElementOpen62541.h"
#include "Registry.h"
#include "devOpcua.h"

// Note: No guard needed for UA_Client_* functions calls because SubscriptionOpen62541 methods
// are either called by UA_Client_run_iterate via SessionOpen62541::connectionStatusChanged
// or via guarded Session methods

namespace DevOpcua {

Registry<SubscriptionOpen62541> SubscriptionOpen62541::subscriptions;

SubscriptionOpen62541::SubscriptionOpen62541 (const std::string &name, SessionOpen62541 &session,
                                      const double publishingInterval, const epicsUInt8 priority,
                                      const int debug)
    : Subscription(name, debug)
    , session(session)
    //TODO: add runtime support for subscription enable/disable
    , requestedSettings(UA_CreateSubscriptionRequest_default())
    , enable(true)
{
    // keep the default timeout
    UA_CreateSubscriptionResponse_init(&subscriptionSettings);
    double deftimeout = requestedSettings.requestedPublishingInterval * requestedSettings.requestedLifetimeCount;
    subscriptionSettings.revisedPublishingInterval = requestedSettings.requestedPublishingInterval = publishingInterval;
    subscriptionSettings.revisedLifetimeCount = requestedSettings.requestedLifetimeCount = static_cast<UA_UInt32>(deftimeout / publishingInterval);
    requestedSettings.priority = priority;

    subscriptions.insert({name, this});
    session.subscriptions[name] = this;
}

void
SubscriptionOpen62541::show (int level) const
{
    std::cout << "subscription=" << name
              << " session="     << session.getName()
              << " interval="    << subscriptionSettings.revisedPublishingInterval
              << "("             << requestedSettings.requestedPublishingInterval  << ")"
              << " prio="        << static_cast<int>(requestedSettings.priority)
              << " enable="    "?" // << (puasubscription ? (puasubscription->publishingEnabled() ? "y" : "n") : "?")
              << "(" << (enable ? "Y" : "N") << ")"
              << " debug=" << debug
              << " items=" << items.size()
              << std::endl;

    if (level >= 1) {
        for (auto &it : items) {
            it->show(level-1);
        }
    }
}

void
SubscriptionOpen62541::showAll (int level)
{
    std::cout << "OPC UA: "
              << subscriptions.size() << " subscription(s) configured"
              << std::endl;
    if (level >= 1) {
        for (auto &it : subscriptions) {
            it.second->show(level-1);
        }
    }
}

Session &
SubscriptionOpen62541::getSession () const
{
    return static_cast<Session &>(session);
}


SessionOpen62541 &
SubscriptionOpen62541::getSessionOpen62541 () const
{
    return session;
}

void
SubscriptionOpen62541::create ()
{
    subscriptionSettings = UA_Client_Subscriptions_create(session.client,
        requestedSettings, this, [] (UA_Client *client, UA_UInt32 subscriptionId,
            void *context, UA_StatusChangeNotification *notification) {
                static_cast<SubscriptionOpen62541*>(context)->
                    subscriptionStatusChanged(notification->status);
            }, NULL);
    if (subscriptionSettings.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        errlogPrintf("OPC UA subscription %s: createSubscription on session %s failed (%s)\n",
                    name.c_str(), session.getName().c_str(),
                    UA_StatusCode_name(subscriptionSettings.responseHeader.serviceResult));
    } else {
        if (debug)
            errlogPrintf("OPC UA subscription %s on session %s created (%s)\n",
                    name.c_str(), session.getName().c_str(),
                    UA_StatusCode_name(subscriptionSettings.responseHeader.serviceResult));
    }
}

void
SubscriptionOpen62541::addMonitoredItems ()
{
    UA_UInt32 i;
    UA_MonitoredItemCreateRequest monitoredItemCreateRequest;
    UA_MonitoredItemCreateResult monitoredItemCreateResult;

    if (items.size()) {
        i = 0;
        for (auto &it : items) {
            UA_MonitoredItemCreateRequest_init(&monitoredItemCreateRequest);
            monitoredItemCreateRequest.itemToMonitor.nodeId = it->getNodeId();
            monitoredItemCreateRequest.itemToMonitor.attributeId = UA_ATTRIBUTEID_VALUE;
            monitoredItemCreateRequest.monitoringMode = UA_MONITORINGMODE_REPORTING;
            monitoredItemCreateRequest.requestedParameters.clientHandle = i;
            monitoredItemCreateRequest.requestedParameters.samplingInterval = it->linkinfo.samplingInterval;
            monitoredItemCreateRequest.requestedParameters.queueSize = it->linkinfo.queueSize;
            monitoredItemCreateRequest.requestedParameters.discardOldest = it->linkinfo.discardOldest;
            monitoredItemCreateResult = UA_Client_MonitoredItems_createDataChange(
                session.client, subscriptionSettings.subscriptionId, UA_TIMESTAMPSTORETURN_BOTH,
                monitoredItemCreateRequest, items[i], [] (UA_Client *client, UA_UInt32 subId, void *subContext,
                         UA_UInt32 monId, void *monContext, UA_DataValue *value) {
                            static_cast<SubscriptionOpen62541*>(subContext)->
                                dataChange(monId, *static_cast<ItemOpen62541*>(monContext), value);
                         }, nullptr /* deleteCallback */);
            if (monitoredItemCreateResult.statusCode == UA_STATUSCODE_GOOD) {
                items[i]->setRevisedSamplingInterval(monitoredItemCreateResult.revisedSamplingInterval);
                items[i]->setRevisedQueueSize(monitoredItemCreateResult.revisedQueueSize);
            }
            if (debug >= 5) {
                if (monitoredItemCreateResult.statusCode == UA_STATUSCODE_GOOD)
                    std::cout << "** Monitored item " << monitoredItemCreateRequest.itemToMonitor.nodeId
                              << " succeeded with id " << monitoredItemCreateResult.monitoredItemId
                              << " revised sampling interval " << monitoredItemCreateResult.revisedSamplingInterval
                              << " revised queue size " << monitoredItemCreateResult.revisedQueueSize
                              << std::endl;
                else
                    std::cout << "** Monitored item " << monitoredItemCreateRequest.itemToMonitor.nodeId
                              << " failed with error "
                              << UA_StatusCode_name(monitoredItemCreateResult.statusCode)
                              << std::endl;
            }
            i++;
        }
        if (debug)
            std::cout << "Subscription " << name << "@" << session.getName()
                      << ": created " << items.size() << " monitored items ("
                      << UA_StatusCode_name(monitoredItemCreateResult.statusCode) << ")" << std::endl;
    }
}

void
SubscriptionOpen62541::clear ()
{
    if(session.client)
        UA_Client_Subscriptions_deleteSingle(session.client, subscriptionSettings.subscriptionId);
}

void
SubscriptionOpen62541::addItemOpen62541 (ItemOpen62541 *item)
{
    items.push_back(item);
}

void
SubscriptionOpen62541::removeItemOpen62541 (ItemOpen62541 *item)
{
    auto it = std::find(items.begin(), items.end(), item);
    if (it != items.end())
        items.erase(it);
}


// callbacks

void
SubscriptionOpen62541::subscriptionStatusChanged (UA_StatusCode status)
{
    errlogPrintf("Subscription %s status changed to %s\n",
        name.c_str(), UA_StatusCode_name(status));
}

void
SubscriptionOpen62541::dataChange (UA_UInt32 monitorId, ItemOpen62541 &item, UA_DataValue *value)
{
    if (debug >= 5) {
        std::cout << "** Subscription " << name
                  << "@" << session.getName()
                  << ": (dataChange) getting data for item " << monitorId
                  << " (" << item.getNodeId();
        if (item.isRegistered() && ! item.linkinfo.identifierIsNumeric)
            std::cout << "/" << item.linkinfo.identifierString;
        std::cout << ")" << std::endl;
    }
    item.setIncomingData(*value, ProcessReason::incomingData);
}

} // namespace DevOpcua
