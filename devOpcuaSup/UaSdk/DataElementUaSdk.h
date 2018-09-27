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

#ifndef DEVOPCUA_DATAELEMENTUASDK_H
#define DEVOPCUA_DATAELEMENTUASDK_H

#include <uadatavalue.h>
#include <statuscode.h>

#include "DataElement.h"
#include "devOpcua.h"
#include "ItemUaSdk.h"
#include "RecordConnector.h"

namespace DevOpcua {

/**
 * @brief The DataElementUaSdk implementation of a single piece of data.
 *
 * See DevOpcua::DataElement
 */
class DataElementUaSdk : public DataElement
{
public:
    /**
     * @brief Constructor for DataElement from record connector.
     *
     * Creates the final (leaf) element of the data structure. The record connector
     * holds a shared pointer to its leaf, while the data element has a weak pointer
     * to the record connector.
     *
     * @param name        name of the element (empty for root element)
     * @param pitem       pointer to corresponding ItemUaSdk
     * @param pconnector  pointer to record connector to link to
     */
    DataElementUaSdk(const std::string &name,
                     ItemUaSdk *pitem,
                     RecordConnector *pconnector);

    /**
     * @brief Constructor for DataElement from child element.
     *
     * Creates an intermediate (node) element of the data structure. The child holds
     * a shared pointer, while the parent has a weak pointer in its list/map of child
     * nodes, to facilitate traversing the structure when data updates come in.
     *
     * @param name   name of the element
     * @param item   pointer to corresponding ItemUaSdk
     * @param child  weak pointer to child
     */
    DataElementUaSdk(const std::string &name,
                     ItemUaSdk *item,
                     std::weak_ptr<DataElementUaSdk> child);

    /**
     * @brief Construct a linked list of data elements between a record connector and an item.
     *
     * Creates the leaf element first, then identifies the part of the path that already exists
     * on the item and creates the missing list of linked nodes.
     *
     * @param pitem       pointer to corresponding ItemUaSdk
     * @param pconnector  pointer to record connector to link to
     * @param path        path of leaf element inside the structure
     */
    static void addElementChain(ItemUaSdk *item,
                                RecordConnector *pconnector,
                                const std::string &path);

    /**
     * @brief Push an incoming data value into the DataElement.
     *
     * Called from the OPC UA client worker thread when new data is
     * received from the OPC UA session.
     *
     * @param value  new value for this data element
     */
    void setIncomingData(const UaDataValue &value);

    /**
     * @brief Get the outgoing data value from the DataElement.
     *
     * Called from the OPC UA client worker thread when data is being
     * assembled in OPC UA session for sending.
     *
     * @param value  new value for this data element
     */
    const UaDataValue &getOutgoingData() { return outgoingData; }

    /**
     * @brief Read the time stamp of the incoming data.
     * See DevOpcua::DataElement::readTimeStamp
     *
     * @param server  true = server time stamp
     * @return EPICS time stamp
     */
    virtual epicsTimeStamp readTimeStamp(bool server = true) const override;

    /**
     * @brief Read incoming data as Int32. See DevOpcua::DataElement::readInt32
     *
     * @return value as epicsInt32
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsInt32 readInt32() const override;

    /**
     * @brief Read incoming data as UInt32. See DevOpcua::DataElement::readUInt32
     *
     * @return value as epicsUInt32
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readUInt32() const override;

    /**
     * @brief Read incoming data as Float64. See DevOpcua::DataElement::readFloat64
     *
     * @return value as epicsFloat64
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsFloat64 readFloat64() const override;

    /**
     * @brief Read incoming data as classic C string (char[]).
     * See DevOpcua::DataElement::readCString
     *
     * @param value  pointer to target string buffer
     * @param num  max no. of bytes to copy (incl. NULL byte)
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual void readCString(char *value, const size_t num) const override;

    /**
     * @brief Check status of last read service. See DevOpcua::DataElement::readWasOk
     *
     * @return true = last read service ok
     */
    virtual bool readWasOk() const override;

    /**
     * @brief Write outgoing Int32 data. See DevOpcua::DataElement::writeInt32
     *
     * @param value  value to write
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeInt32(const epicsInt32 &value) override;

    /**
     * @brief Write outgoing UInt32 data. See DevOpcua::DataElement::writeUInt32
     *
     * @param value  value to write
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeUInt32(const epicsUInt32 &value) override;

    /**
     * @brief Write outgoing Float64 data. See DevOpcua::DataElement::writeFloat64
     *
     * @param value  value to write
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeFloat64(const epicsFloat64 &value) override;

    /**
     * @brief Write outgoing classic C string (char[]) data.
     * See DevOpcua::DataElement::writeCString
     *
     * @param value  pointer to source string buffer
     * @param num  max no. of bytes to copy (incl. NULL byte)
     * @throws std::runtime_error on conversion error
     */
    virtual void writeCString(const char *value, const size_t num) override;

    /**
     * @brief Check status of last write service. See DevOpcua::DataElement::writeOk
     *
     * @return true = last write service ok
     */
    virtual bool writeWasOk() const override;

    /**
     * @brief Clear (discard) the current incoming data.
     * See DevOpcua::DataElement::clearIncomingData
     */
    virtual void clearIncomingData() override;

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
    virtual void clearOutgoingData() { outgoingData.clear(); }

    /**
     * @brief Create processing requests for record(s) attached to this element.
     * See DevOpcua::DataElement::requestRecordProcessing
     */
    virtual void requestRecordProcessing(const ProcessReason reason) const override;

private:
    static void printOutputDebugMessage(const RecordConnector *pconnector,
                                        const UaVariant &tempValue);

    ItemUaSdk *pitem;                                       /**< corresponding item */
    std::vector<std::weak_ptr<DataElementUaSdk>> elements;  /**< children (if node) */
    std::shared_ptr<DataElementUaSdk> parent;               /**< parent */

    UaDataValue incomingData;        /**< incoming data value */
    OpcUa_BuiltInType incomingType;  /**< type of incoming data */
    UaDataValue outgoingData;        /**< outgoing data value */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTUASDK_H
