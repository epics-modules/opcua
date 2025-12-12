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

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#endif

#include <uaclientsdk.h>
#include <uasession.h>
#include <opcua_types.h>

#include <errlog.h>
#include <epicsThread.h>
#include <epicsEvent.h>

#define epicsExportSharedSymbols
#include "SubscriptionUaSdk.h"
#include "RecordConnector.h"
#include "ItemUaSdk.h"
#include "DataElementUaSdk.h"
#include "Registry.h"
#include "devOpcua.h"

namespace DevOpcua {

using namespace UaClientSdk;

Registry<SubscriptionUaSdk> SubscriptionUaSdk::subscriptions;

SubscriptionUaSdk::SubscriptionUaSdk (const std::string &name, SessionUaSdk *session,
                                      const double publishingInterval)
    : Subscription(name)
    , puasubscription(nullptr)
    , psessionuasdk(session)
    //TODO: add runtime support for subscription enable/disable
    , enable(true)
{
    // keep the default timeout
    double deftimeout = subscriptionSettings.publishingInterval * subscriptionSettings.lifetimeCount;
    subscriptionSettings.publishingInterval = requestedSettings.publishingInterval = publishingInterval;
    subscriptionSettings.lifetimeCount = requestedSettings.lifetimeCount = static_cast<OpcUa_UInt32>(deftimeout / publishingInterval);

    subscriptions.insert({name, this});
    psessionuasdk->subscriptions[name] = this;
}

void SubscriptionUaSdk::setOption(const std::string &name, const std::string &value)
{
    if (debug || name == "debug")
        std::cerr << "Subscription " << this->name << ": setting option " << name << " to " << value
                  << std::endl;

    if (name == "debug") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        debug = ul;
    } else if (name == "priority") {
        unsigned long ul = std::strtoul(value.c_str(), nullptr, 0);
        if (ul > 255ul)
            errlogPrintf("option '%s' value out of range - ignored\n", name.c_str());
        else
            subscriptionSettings.priority = requestedSettings.priority = static_cast<OpcUa_Byte>(ul);
    } else {
        errlogPrintf("unknown option '%s' - ignored\n", name.c_str());
    }
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
            if (it->linkinfo.deadband > 0.0) {
#ifdef _WIN32
                errlogPrintf("OPC UA subscription %s@%s: deadband not implemented for UA SDK on Windows\n",
                    name.c_str(), psessionuasdk->getName().c_str());
#else
                OpcUa_DataChangeFilter* pDataChangeFilter = NULL;
                status = OpcUa_EncodeableObject_CreateExtension(
                    &OpcUa_DataChangeFilter_EncodeableType,
                    &monitoredItemCreateRequests[i].RequestedParameters.Filter,
                    (OpcUa_Void**)&pDataChangeFilter);
                if (status.isBad()) {
                    errlogPrintf("OPC UA subscription %s@%s: cannot create deadband filter: %s\n",
                    name.c_str(), psessionuasdk->getName().c_str(), status.toString().toUtf8());
                } else {
                    pDataChangeFilter->DeadbandType = OpcUa_DeadbandType_Absolute;
                    pDataChangeFilter->DeadbandValue = it->linkinfo.deadband;
                    pDataChangeFilter->Trigger = OpcUa_DataChangeTrigger_StatusValue;
                }
#endif
            }
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
            for (i = 0; i < items.size(); i++) {
                if (OpcUa_IsGood(monitoredItemCreateResults[i].StatusCode)) {
                    if (debug >= 5)
                        std::cout << "** OPC UA record " << items[i]->recConnector->getRecordName()
                                  << " monitored item " << UaNodeId(monitoredItemCreateRequests[i].ItemToMonitor.NodeId).toXmlString().toUtf8()
                                  << " succeeded with id " << monitoredItemCreateResults[i].MonitoredItemId
                                  << " revised sampling interval " << monitoredItemCreateResults[i].RevisedSamplingInterval
                                  << " revised queue size " << monitoredItemCreateResults[i].RevisedQueueSize
                                  << std::endl;
                } else {
                    errlogPrintf("OPC UA record %s monitored item %s failed with error %s\n",
                        items[i]->recConnector->getRecordName(),
                        UaNodeId(monitoredItemCreateRequests[i].ItemToMonitor.NodeId).toXmlString().toUtf8(),
                        UaStatus(monitoredItemCreateResults[i].StatusCode).toString().toUtf8());
                    items[i]->setIncomingEvent(ProcessReason::connectionLoss);
                }
            }
        }
    }
}

void
SubscriptionUaSdk::clear ()
{
    puasubscription = nullptr;
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
                      << " (" << item->getNodeId().toXmlString().toUtf8();
            if (item->isRegistered() && ! item->linkinfo.identifierIsNumeric)
                std::cout << "/" << item->linkinfo.identifierString;
            std::cout << ")" << std::endl;
        }
        item->setIncomingData(dataNotifications[i].Value, ProcessReason::incomingData);
    }
}

void
SubscriptionUaSdk::newEvents (OpcUa_UInt32 clientSubscriptionHandle,
                              UaEventFieldLists& eventFieldList)
{}

} // namespace DevOpcua
