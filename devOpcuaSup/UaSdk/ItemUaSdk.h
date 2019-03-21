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

#include "Item.h"
#include "opcuaItemRecord.h"
#include "devOpcua.h"
#include "SessionUaSdk.h"

namespace DevOpcua {

using namespace UaClientSdk;

class SubscriptionUaSdk;
class DataElementUaSdk;
class RecordConnector;
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
    void setReadStatus(const OpcUa_StatusCode &status) { readStatus = status; }

    /**
     * @brief Getter for the status of the last read operation.
     * @return read status
     */
    const UaStatusCode &getReadStatus() { return readStatus; }

    /**
     * @brief Setter for the status of a write operation.
     * @param status  status code received by the client library
     */
    void setWriteStatus(const OpcUa_StatusCode &status) { writeStatus = status; }

    /**
     * @brief Getter for the status of the last write operation.
     * @return write status
     */
    const UaStatusCode &getWriteStatus() { return writeStatus; }

    /**
     * @brief Get a structure definition from the session dictionary.
     * @param dataTypeId data type of the extension object
     * @return structure definition
     */
    UaStructureDefinition structureDefinition(const UaNodeId &dataTypeId)
    { return session->structureDefinition(dataTypeId); }

    /**
     * @brief Create processing requests for record(s) attached to this item.
     * See DevOpcua::DataElement::requestRecordProcessing
     */
    void requestRecordProcessing(const ProcessReason reason) const;

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
     */
    void setIncomingData(const OpcUa_DataValue &value);

    /**
     * @brief Convert OPC UA time stamp to EPICS time stamp.
     * @param dt time stamp in UaDateTime format
     * @param pico10 10 picosecond resolution counter
     * @return EPICS time stamp
     */
    static epicsTimeStamp uaToEpicsTimeStamp(const UaDateTime &dt, const OpcUa_UInt16 pico10);

    /**
     * @brief Get debug level (from itemRecord or via TOP DataElement)
     * @return debug level
     */
    int debug() const;

private:
    SubscriptionUaSdk *subscription;   /**< raw pointer to subscription (if monitored) */
    SessionUaSdk *session;             /**< raw pointer to session */
    std::unique_ptr<UaNodeId> nodeid;  /**< node id of this item */
    bool registered;                   /**< flag for registration status */
    std::weak_ptr<DataElementUaSdk> rootElement;  /**< top level data element */
    UaStatusCode readStatus;           /**< status code of last read service */
    UaStatusCode writeStatus;          /**< status code of last write service */
    epicsTimeStamp tsServer;           /**< server time stamp */
    epicsTimeStamp tsSource;           /**< device time stamp */
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEMUASDK_H
