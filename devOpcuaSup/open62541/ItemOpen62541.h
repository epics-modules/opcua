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

#ifndef DEVOPCUA_ITEMOPEN62541_H
#define DEVOPCUA_ITEMOPEN62541_H

#include <memory>

#include <epicsTime.h>

#include <open62541/client.h>

#include "Item.h"
#include "opcuaItemRecord.h"
#include "devOpcua.h"
#include "ElementTree.h"
#include "SessionOpen62541.h"

namespace DevOpcua {

class SubscriptionOpen62541;
class DataElementOpen62541;
struct linkInfo;

/**
 * @brief The ItemOpen62541 inplementation of an OPC UA item.
 *
 * See DevOpcua::Item
 */
class ItemOpen62541 : public Item
{
    friend class DataElementOpen62541;

public:
    /**
     * @brief Constructor for an OPC UA item (implementation).
     * @param info  configuration as parsed from the EPICS database
     */
    ItemOpen62541(const linkInfo &info);
    ~ItemOpen62541() override;

    /**
     * @brief Rebuild the node id from link info structure.
     * @param info  configuration as parsed from the EPICS database
     */
    void rebuildNodeId();

    /**
     * @brief Request beginRead service. See DevOpcua::Item::requestRead
     */
    virtual void requestRead() override { session->requestRead(*this); }

    /**
     * @brief Request beginWrite service. See DevOpcua::Item::requestWrite
     */
    virtual void requestWrite() override { session->requestWrite(*this); }

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
     * @brief Get the EPICS-related state of the item.
     * See DevOpcua::Item::state
     */
    virtual ConnectionStatus state() const override { return connState; }

    /**
     * @brief Set the EPICS-related state of the item.
     * See DevOpcua::Item::setState
     */
    virtual void setState(const ConnectionStatus state) override { connState = state; }

    /**
     * @brief Return registered status.
     */
    bool isRegistered() const { return registered; }

    /**
     * @brief Setter for the node id of this item.
     * @return node id
     */
    void setRegisteredNodeId(const UA_NodeId &id) { UA_NodeId_copy(&id, &nodeid); registered = true; }

    /**
     * @brief Getter that returns the node id of this item.
     * @return node id
     */
    const UA_NodeId &getNodeId() const { return nodeid; }

    /**
     * @brief Setter for the status of a read operation.
     * @param status  status code received by the client library
     */
    void setLastStatus(const UA_StatusCode &status) { lastStatus = status; }

    /**
     * @brief Getter for the status of the last read operation.
     * @return read status
     */
    UA_StatusCode getLastStatus() { return lastStatus; }

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
//    UaStructureDefinition structureDefinition(const UA_NodeId &dataTypeId)
//    { return session->structureDefinition(dataTypeId); }

    /**
     * @brief Get the outgoing data value.
     *
     * Called from the OPC UA client worker thread when data is being
     * assembled in OPC UA session for sending.
     */
    const UA_Variant &getOutgoingData() const;

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
    void setIncomingData(const UA_DataValue &value, ProcessReason reason);

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
    void setRevisedSamplingInterval(const UA_Double &interval)
    { revisedSamplingInterval = interval; }

    /**
     * @brief Setter for the revised sampling interval.
     * @param status  status code received by the client library
     */
    void setRevisedQueueSize(const UA_UInt32 &qsize)
    { revisedQueueSize = qsize; }

    /**
     * @brief Convert OPC UA time stamp to EPICS time stamp.
     * @param dt time stamp in UaDateTime format
     * @param pico10 10 picosecond resolution counter
     * @return EPICS time stamp
     */
    static epicsTime uaToEpicsTime(const UA_DateTime &dt, const UA_UInt16 pico10);

    /**
     * @brief Get debug level (from itemRecord or via TOP DataElement)
     * @return debug level
     */
    int debug() const;

private:
    SubscriptionOpen62541 *subscription;   /**< raw pointer to subscription (if monitored) */
    SessionOpen62541 *session;             /**< raw pointer to session */
    UA_NodeId nodeid;                      /**< node id of this item */
    bool registered;                       /**< flag for registration status */
    UA_Double revisedSamplingInterval;     /**< server-revised sampling interval */
    UA_UInt32 revisedQueueSize;            /**< server-revised queue size */
    ElementTree<DataElementOpen62541, ItemOpen62541> dataTree; /**< data element tree */
    UA_StatusCode lastStatus;              /**< status code of most recent service */
    ProcessReason lastReason;              /**< most recent processing reason */
    ConnectionStatus connState;            /**< Connection state of the item */
    epicsTime tsClient;                    /**< client (local) time stamp */
    epicsTime tsServer;                    /**< server time stamp */
    epicsTime tsSource;                    /**< device time stamp */
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEMOPEN62541_H
