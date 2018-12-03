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

#include <iostream>
#include <string>
#include <map>

#include <uaclientsdk.h>
#include <uasession.h>

#include <errlog.h>

#define epicsExportSharedSymbols
#include "SubscriptionUaSdk.h"
#include "ItemUaSdk.h"
#include "DataElementUaSdk.h"
#include "devOpcua.h"

namespace DevOpcua {

using namespace UaClientSdk;

std::map<std::string, SubscriptionUaSdk*> SubscriptionUaSdk::subscriptions;

SubscriptionUaSdk::SubscriptionUaSdk (const std::string &name, SessionUaSdk *session,
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
    subscriptionSettings.publishingInterval = publishingInterval;
    subscriptionSettings.lifetimeCount = static_cast<OpcUa_UInt32>(deftimeout / publishingInterval);
    subscriptionSettings.priority = priority;

    subscriptions[name] = this;
    psessionuasdk->subscriptions[name] = this;
}

void
SubscriptionUaSdk::show (int level) const
{
    std::cout << "subscription=" << name
              << " session="     << psessionuasdk->getName()
              << " interval=";
    if (puasubscription)
        std::cout << puasubscription->publishingInterval();
    else
        std::cout << "?";
    std::cout << "(" << subscriptionSettings.publishingInterval << ")"
              << " prio=";
    if (puasubscription)
        std::cout << static_cast<int>(puasubscription->priority());
    else
        std::cout << "?";
    std::cout << "(" << static_cast<int>(subscriptionSettings.priority) << ")"
              << " enable=" << (puasubscription ? (puasubscription->publishingEnabled() ? "Y" : "N") : "?")
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

SubscriptionUaSdk &
SubscriptionUaSdk::findSubscription (const std::string &name)
{
    auto it = subscriptions.find(name);
    if (it == subscriptions.end()) {
        throw std::runtime_error("no such subscription");
    }
    return *(it->second);
}

bool
SubscriptionUaSdk::subscriptionExists (const std::string &name)
{
    auto it = subscriptions.find(name);
    return !(it == subscriptions.end());
}

void
SubscriptionUaSdk::showAll (int level)
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
SubscriptionUaSdk::getSession () const
{
    return static_cast<Session &>(*psessionuasdk);
}


SessionUaSdk &
SubscriptionUaSdk::getSessionUaSdk () const
{
    return *psessionuasdk;
}

void
SubscriptionUaSdk::create ()
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
SubscriptionUaSdk::addMonitoredItems ()
{
    UaStatus status;
    ServiceSettings serviceSettings;
    OpcUa_UInt32 i;
    UaMonitoredItemCreateRequests monitoredItemCreateRequests;
    UaMonitoredItemCreateResults monitoredItemCreateResults;

    monitoredItemCreateRequests.create(items.size());
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

void
SubscriptionUaSdk::clear ()
{
    puasubscription = NULL;
}

void
SubscriptionUaSdk::addItemUaSdk (ItemUaSdk *item)
{
    items.push_back(item);
}

void
SubscriptionUaSdk::removeItemUaSdk (ItemUaSdk *item)
{
    auto it = std::find(items.begin(), items.end(), item);
    if (it != items.end())
        items.erase(it);
}


// UaSubscriptionCallback interface

void
SubscriptionUaSdk::subscriptionStatusChanged (OpcUa_UInt32 clientSubscriptionHandle,
                                              const UaStatus& status)
{}

void
SubscriptionUaSdk::dataChange (OpcUa_UInt32 clientSubscriptionHandle,
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
        ItemUaSdk *item = items[dataNotifications[i].ClientHandle];
        if (debug >= 5) {
            std::cout << "** Subscription " << name.c_str()
                      << "@" << psessionuasdk->getName()
                      << ": (dataChange) getting data for item " << dataNotifications[i].ClientHandle
                      << " (" << item->getNodeId().toXmlString().toUtf8() << ")" << std::endl;
        }
        item->setReadStatus(dataNotifications[i].Value.StatusCode);
        item->data().setIncomingData(dataNotifications[i].Value);
        item->data().requestRecordProcessing(ProcessReason::incomingData);
    }
}

void
SubscriptionUaSdk::newEvents (OpcUa_UInt32  clientSubscriptionHandle,
                              UaEventFieldLists&          eventFieldList)
{}

} // namespace DevOpcua
