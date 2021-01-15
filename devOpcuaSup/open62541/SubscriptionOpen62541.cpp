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

#include <iostream>
#include <string>
#include <map>

#include <uaclientsdk.h>
#include <uasession.h>

#include <errlog.h>

#define epicsExportSharedSymbols
#include "SubscriptionOpen62541.h"
#include "ItemOpen62541.h"
#include "DataElementOpen62541.h"
#include "devOpcua.h"

namespace DevOpcua {

std::map<std::string, SubscriptionOpen62541*> SubscriptionOpen62541::subscriptions;

SubscriptionOpen62541::SubscriptionOpen62541 (const std::string &name, SessionOpen62541 *session,
                                      const double publishingInterval, const epicsUInt8 priority,
                                      const int debug)
    : Subscription(name, debug)
    , puasubscription(nullptr)
    , psessionuasdk(session)
    //TODO: add runtime support for subscription enable/disable
    , enable(true)
{
    // keep the default timeout
    double deftimeout = subscriptionSettings.publishingInterval * subscriptionSettings.lifetimeCount;
    subscriptionSettings.publishingInterval = requestedSettings.publishingInterval = publishingInterval;
    subscriptionSettings.lifetimeCount = requestedSettings.lifetimeCount = static_cast<OpcUa_UInt32>(deftimeout / publishingInterval);
    subscriptionSettings.priority = requestedSettings.priority = priority;

    subscriptions[name] = this;
    psessionuasdk->subscriptions[name] = this;
}

void
SubscriptionOpen62541::show (int level) const
{
    std::cout << "subscription=" << name
              << " session="     << psessionuasdk->getName()
              << " interval=";
    if (puasubscription)
        std::cout << puasubscription->publishingInterval();
    else
        std::cout << "?";
    std::cout << "(" << requestedSettings.publishingInterval << ")"
              << " prio=";
    if (puasubscription)
        std::cout << static_cast<int>(puasubscription->priority());
    else
        std::cout << "?";
    std::cout << "(" << static_cast<int>(requestedSettings.priority) << ")"
              << " enable=" << (puasubscription ? (puasubscription->publishingEnabled() ? "y" : "n") : "?")
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
    return static_cast<Session &>(*psessionuasdk);
}


SessionOpen62541 &
SubscriptionOpen62541::getSessionOpen62541 () const
{
    return *psessionuasdk;
}

void
SubscriptionOpen62541::create ()
{
    UaStatus status;
    ServiceSettings serviceSettings;

    status = psessionuasdk->puasession->createSubscription(
                serviceSettings,
                this,
                0,
                subscriptionSettings,
                enable,
                &puasubscription);

    if (status.isBad()) {
        errlogPrintf("OPC UA subscription %s: createSubscription on session %s failed (%s)\n",
                     name.c_str(), psessionuasdk->getName().c_str(), status.toString().toUtf8());
    } else {
        if (debug)
            errlogPrintf("OPC UA subscription %s on session %s created (%s)\n",
                         name.c_str(), psessionuasdk->getName().c_str(), status.toString().toUtf8());
    }
}

void
SubscriptionOpen62541::addMonitoredItems ()
{
    UaStatus status;
    ServiceSettings serviceSettings;
    OpcUa_UInt32 i;
    UaMonitoredItemCreateRequests monitoredItemCreateRequests;
    UaMonitoredItemCreateResults monitoredItemCreateResults;

    if (items.size()) {
        monitoredItemCreateRequests.create(static_cast<OpcUa_UInt32>(items.size()));
        i = 0;
        for (auto &it : items) {
            it->getNodeId().copyTo(&monitoredItemCreateRequests[i].ItemToMonitor.NodeId);
            monitoredItemCreateRequests[i].ItemToMonitor.AttributeId = OpcUa_Attributes_Value;
            monitoredItemCreateRequests[i].MonitoringMode = OpcUa_MonitoringMode_Reporting;
            monitoredItemCreateRequests[i].RequestedParameters.ClientHandle = i;
            monitoredItemCreateRequests[i].RequestedParameters.SamplingInterval = it->linkinfo.samplingInterval;
            monitoredItemCreateRequests[i].RequestedParameters.QueueSize = it->linkinfo.queueSize;
            monitoredItemCreateRequests[i].RequestedParameters.DiscardOldest = it->linkinfo.discardOldest;
            i++;
        }

        status = puasubscription->createMonitoredItems(
                    serviceSettings,               // Use default settings
                    OpcUa_TimestampsToReturn_Both, // Select timestamps to return
                    monitoredItemCreateRequests,   // monitored items to create
                    monitoredItemCreateResults);   // Returned monitored items create result

        if (status.isBad()) {
            errlogPrintf("OPC UA subscription %s@%s: createMonitoredItems failed with status %s\n",
                         name.c_str(), psessionuasdk->getName().c_str(), status.toString().toUtf8());
        } else {
            for (i = 0; i < items.size(); i++) {
                items[i]->setRevisedSamplingInterval(monitoredItemCreateResults[i].RevisedSamplingInterval);
                items[i]->setRevisedQueueSize(monitoredItemCreateResults[i].RevisedQueueSize);
            }
            if (debug)
                std::cout << "Subscription " << name << "@" << psessionuasdk->getName()
                          << ": created " << items.size() << " monitored items ("
                          << status.toString().toUtf8() << ")" << std::endl;
            if (debug >= 5) {
                for (i = 0; i < items.size(); i++) {
                    UaNodeId node(monitoredItemCreateRequests[i].ItemToMonitor.NodeId);
                    if (OpcUa_IsGood(monitoredItemCreateResults[i].StatusCode))
                        std::cout << "** Monitored item " << node.toXmlString().toUtf8()
                                  << " succeeded with id " << monitoredItemCreateResults[i].MonitoredItemId
                                  << " revised sampling interval " << monitoredItemCreateResults[i].RevisedSamplingInterval
                                  << " revised queue size " << monitoredItemCreateResults[i].RevisedQueueSize
                                  << std::endl;
                    else
                        std::cout << "** Monitored item " << node.toXmlString().toUtf8()
                                  << " failed with error "
                                  << UaStatus(monitoredItemCreateResults[i].StatusCode).toString().toUtf8()
                                  << std::endl;
                }
            }
        }
    }
}

void
SubscriptionOpen62541::clear ()
{
    puasubscription = nullptr;
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


// UaSubscriptionCallback interface

void
SubscriptionOpen62541::subscriptionStatusChanged (OpcUa_UInt32 clientSubscriptionHandle,
                                              const UaStatus& status)
{}

void
SubscriptionOpen62541::dataChange (OpcUa_UInt32 clientSubscriptionHandle,
                               const UaDataNotifications& dataNotifications,
                               const UaDiagnosticInfos&   diagnosticInfos)
{
    OpcUa_UInt32 i;

    if (debug)
        std::cout << "Subscription " << name.c_str()
                  << "@" << psessionuasdk->getName()
                  << ": (dataChange) getting data for "
                  << dataNotifications.length() << " items" << std::endl;

    for (i = 0; i < dataNotifications.length(); i++) {
        ItemOpen62541 *item = items[dataNotifications[i].ClientHandle];
        if (debug >= 5) {
            std::cout << "** Subscription " << name.c_str()
                      << "@" << psessionuasdk->getName()
                      << ": (dataChange) getting data for item " << dataNotifications[i].ClientHandle
                      << " (" << item->getNodeId().toXmlString().toUtf8();
            if (item->isRegistered() && ! item->linkinfo.identifierIsNumeric)
                std::cout << "/" << item->linkinfo.identifierString;
            std::cout << ")" << std::endl;
        }
        item->setIncomingData(dataNotifications[i].Value, ProcessReason::incomingData);
    }
}

void
SubscriptionOpen62541::newEvents (OpcUa_UInt32 clientSubscriptionHandle,
                              UaEventFieldLists& eventFieldList)
{}

} // namespace DevOpcua
