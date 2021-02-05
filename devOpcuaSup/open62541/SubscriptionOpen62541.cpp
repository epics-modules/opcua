/*************************************************************************\
* Copyright (c) 2018-2019 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#include <iostream>
#include <string>
#include <map>

#include <open62541/client_subscriptions.h>

#include <errlog.h>

#define epicsExportSharedSymbols
#include "SubscriptionOpen62541.h"
#include "ItemOpen62541.h"
#include "DataElementOpen62541.h"
#include "devOpcua.h"

namespace DevOpcua {

std::map<std::string, SubscriptionOpen62541*> SubscriptionOpen62541::subscriptions;

SubscriptionOpen62541::SubscriptionOpen62541 (const std::string &name, SessionOpen62541 &session,
                                      const double publishingInterval, const epicsUInt8 priority,
                                      const int debug)
    : Subscription(name, debug)
//    , puasubscription(nullptr)
    , session(session)
    //TODO: add runtime support for subscription enable/disable
    , requestedSettings(UA_CreateSubscriptionRequest_default())
    , enable(true)
{
    // keep the default timeout
    double deftimeout = requestedSettings.requestedPublishingInterval * requestedSettings.requestedLifetimeCount;
    subscriptionSettings.revisedPublishingInterval = requestedSettings.requestedPublishingInterval = publishingInterval;
    subscriptionSettings.revisedLifetimeCount = requestedSettings.requestedLifetimeCount = static_cast<UA_UInt32>(deftimeout / publishingInterval);
    requestedSettings.priority = priority;

    subscriptions[name] = this;
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

SubscriptionOpen62541 &
SubscriptionOpen62541::findSubscription (const std::string &name)
{
    auto it = subscriptions.find(name);
    if (it == subscriptions.end()) {
        throw std::runtime_error("no such subscription");
    }
    return *(it->second);
}

bool
SubscriptionOpen62541::subscriptionExists (const std::string &name)
{
    auto it = subscriptions.find(name);
    return !(it == subscriptions.end());
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
    errlogPrintf("SubscriptionOpen62541::create %s on session %s\n", name.c_str(), session.getName().c_str());

    subscriptionSettings = UA_Client_Subscriptions_create(session.client,
        requestedSettings, this, [] (UA_Client *client, UA_UInt32 subscriptionId,
            void *context, UA_StatusChangeNotification *notification) {
                static_cast<SubscriptionOpen62541*>(context)->
                    subscriptionStatusChanged(subscriptionId, notification->status);
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

static void UA_dataChange(UA_Client *client, UA_UInt32 subId, void *subContext,
                         UA_UInt32 monId, void *monContext, UA_DataValue *value)
{
    errlogPrintf("data changed subId=%u subContext=%p, monId=%u, monContext=%p, value at %p\n",
        subId, subContext, monId, monContext, value);
}

void
SubscriptionOpen62541::addMonitoredItems ()
{
    UA_UInt32 i;
    UA_MonitoredItemCreateRequest monitoredItemCreateRequest;
    UA_MonitoredItemCreateResult monitoredItemCreateResult;

    errlogPrintf("SubscriptionOpen62541::addMonitoredItems [%zu]\n", items.size());

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
                monitoredItemCreateRequest,
                nullptr, UA_dataChange, nullptr);

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
    errlogPrintf("SubscriptionOpen62541::addMonitoredItems DONE\n");
}

void
SubscriptionOpen62541::clear ()
{
//    puasubscription = nullptr;
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
SubscriptionOpen62541::subscriptionInactive(UA_UInt32 subscriptionId)
{
    errlogPrintf("Subscription %s=%u inactive\n",
        name.c_str(), subscriptionId);
}

void
SubscriptionOpen62541::subscriptionStatusChanged (UA_UInt32 subscriptionId,
                                              UA_StatusCode status)
{
    errlogPrintf("Subscription %s=%u status changed to %s\n",
        name.c_str(), subscriptionId, UA_StatusCode_name(status));
}

/*
void
SubscriptionOpen62541::dataChange (UA_UInt32 clientSubscriptionHandle,
                               const UaDataNotifications& dataNotifications,
                               const UaDiagnosticInfos&   diagnosticInfos)
{
    UA_UInt32 i;

    if (debug)
        std::cout << "Subscription " << name.c_str()
                  << "@" << session.getName()
                  << ": (dataChange) getting data for "
                  << dataNotifications.length() << " items" << std::endl;

    for (i = 0; i < dataNotifications.length(); i++) {
        ItemOpen62541 *item = items[dataNotifications[i].ClientHandle];
        if (debug >= 5) {
            std::cout << "** Subscription " << name.c_str()
                      << "@" << session.getName()
                      << ": (dataChange) getting data for item " << dataNotifications[i].ClientHandle
                      << " (" << item->getNodeId().toXmlString().toUtf8();
            if (item->isRegistered() && ! item->linkinfo.identifierIsNumeric)
                std::cout << "/" << item->linkinfo.identifierString;
            std::cout << ")" << std::endl;
        }
        item->setIncomingData(dataNotifications[i].Value, ProcessReason::incomingData);
    }
}
*/

/*
void
SubscriptionOpen62541::newEvents (UA_UInt32 clientSubscriptionHandle,
                              UaEventFieldLists& eventFieldList)
{}
*/

} // namespace DevOpcua
