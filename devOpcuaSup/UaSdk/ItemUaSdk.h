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

#ifndef DEVOPCUA_ITEMUASDK_H
#define DEVOPCUA_ITEMUASDK_H

#include <memory>

#include <statuscode.h>
#include <opcua_builtintypes.h>
#include <uastructuredefinition.h>

#include <epicsTime.h>

#include "Item.h"
#include "opcuaItemRecord.h"
#include "devOpcua.h"
#include "SessionUaSdk.h"

namespace DevOpcua {

using namespace UaClientSdk;

class SubscriptionUaSdk;
class DataElementUaSdk;
struct linkInfo;

/**
 * @brief The ItemUaSdk inplementation of an OPC UA item.
 *
 * See DevOpcua::Item
 */
class ItemUaSdk : public Item
{
    friend class DataElementUaSdk;

public:
    /**
     * @brief Constructor for an OPC UA item (implementation).
     * @param info  configuration as parsed from the EPICS database
     */
    ItemUaSdk(const linkInfo &info);
    ~ItemUaSdk() override;

    /**
     * @brief Rebuild the node id from link info structure.
     * @param info  configuration as parsed from the EPICS database
     */
    void rebuildNodeId();

    /**
     * @brief Request beginRead service. See DevOpcua::Item::requestRead
     */
    virtual void requestRead(const unsigned short priority) override { session->requestRead(*this, priority); }

    /**
     * @brief Request beginWrite service. See DevOpcua::Item::requestWrite
     */
    virtual void requestWrite(const unsigned short priority) override { session->requestWrite(*this, priority); }

    /**
     * @brief Print configuration and status. See DevOpcua::Item::show
     */
    virtual void show(int level) const override;

    /**
     * @brief Return monitored status. See DevOpcua::Item::isMonitored
     */
    virtual bool isMonitored() const override { return !!subscription; }

    /**
     * @brief Return OPC UA status code and text.
     * See DevOpcua::Item::getStatus
     */
    virtual void getStatus(epicsUInt32 *code,
                           char *text = nullptr,
                           const epicsUInt32 len = 0,
                           epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Return registered status.
     */
    bool isRegistered() const { return registered; }

    /**
     * @brief Setter for the node id of this item.
     * @return node id
     */
    void setRegisteredNodeId(const UaNodeId &id) { (*nodeid) = id; registered = true; }

    /**
     * @brief Getter that returns the node id of this item.
     * @return node id
     */
    UaNodeId &getNodeId() const { return (*nodeid); }

    /**
     * @brief Setter for the status of a read operation.
     * @param status  status code received by the client library
     */
    void setLastStatus(const OpcUa_StatusCode &status) { lastStatus = status; }

    /**
     * @brief Getter for the status of the last read operation.
     * @return read status
     */
    UaStatusCode getLastStatus() { return lastStatus; }

    /**
     * @brief Setter for the reason of an operation.
     * @param reason  new reason to be cached
     */
    void setReason(const ProcessReason reason) { lastReason = reason; }

    /**
     * @brief Getter for the reason of the most recent operation.
     * @return process reason
     */
    ProcessReason getReason() { return lastReason; }

    /**
     * @brief Get a structure definition from the session dictionary.
     * @param dataTypeId data type of the extension object
     * @return structure definition
     */
    UaStructureDefinition structureDefinition(const UaNodeId &dataTypeId)
    { return session->structureDefinition(dataTypeId); }

    /**
     * @brief Get the outgoing data value.
     *
     * Called from the OPC UA client worker thread when data is being
     * assembled in OPC UA session for sending.
     */
    const UaVariant &getOutgoingData() const;

    /**
     * @brief Clear (discard) the current outgoing data.
     *
     * Called by the low level connection (OPC UA session)
     * after it is done accessing the data in the context of sending.
     *
     * In case an implementation uses a queue, this should remove the
     * oldest element from the queue, allowing access to the next element
     * with the next send.
     */
    void clearOutgoingData();

    /**
     * @brief Push an incoming data value down the root element.
     *
     * Called from the OPC UA client worker thread when new data is
     * received from the OPC UA session.
     *
     * @param value  new value for this data element
     * @param reason  reason for this value update
     */
    void setIncomingData(const OpcUa_DataValue &value, ProcessReason reason);

    /**
     * @brief Push an incoming event down the root element.
     *
     * Called from the OPC UA client worker thread when an out-of-band
     * event was received (connection loss).
     *
     * @param reason  reason for this value update
     */
    void setIncomingEvent(ProcessReason reason);

    /**
     * @brief Setter for the revised sampling interval.
     * @param status  status code received by the client library
     */
    void setRevisedSamplingInterval(const OpcUa_Double &interval)
    { revisedSamplingInterval = interval; }

    /**
     * @brief Setter for the revised sampling interval.
     * @param status  status code received by the client library
     */
    void setRevisedQueueSize(const OpcUa_UInt32 &qsize)
    { revisedQueueSize = qsize; }

    /**
     * @brief Convert OPC UA time stamp to EPICS time stamp.
     * @param dt time stamp in UaDateTime format
     * @param pico10 10 picosecond resolution counter
     * @return EPICS time stamp
     */
    static epicsTime uaToEpicsTime(const UaDateTime &dt, const OpcUa_UInt16 pico10);

    /**
     * @brief Get debug level (from itemRecord or via TOP DataElement)
     * @return debug level
     */
    int debug() const;

private:
    SubscriptionUaSdk *subscription;       /**< raw pointer to subscription (if monitored) */
    SessionUaSdk *session;                 /**< raw pointer to session */
    std::unique_ptr<UaNodeId> nodeid;      /**< node id of this item */
    bool registered;                       /**< flag for registration status */
    OpcUa_Double revisedSamplingInterval;  /**< server-revised sampling interval */
    OpcUa_UInt32 revisedQueueSize;         /**< server-revised queue size */
    std::weak_ptr<DataElementUaSdk> rootElement;  /**< top level data element */
    UaStatusCode lastStatus;               /**< status code of most recent service */
    ProcessReason lastReason;              /**< most recent processing reason */
    epicsTime tsClient;                    /**< client (local) time stamp */
    epicsTime tsServer;                    /**< server time stamp */
    epicsTime tsSource;                    /**< device time stamp */
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEMUASDK_H
