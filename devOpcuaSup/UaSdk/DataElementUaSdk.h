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

#ifndef DEVOPCUA_DATAELEMENTUASDK_H
#define DEVOPCUA_DATAELEMENTUASDK_H

#include <unordered_map>

#include <uadatavalue.h>
#include <statuscode.h>

#include "DataElement.h"
#include "devOpcua.h"
#include "RecordConnector.h"
#include "UpdateQueue.h"
#include "ItemUaSdk.h"

namespace DevOpcua {

class ItemUaSdk;

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
     * @brief Print configuration and status. See DevOpcua::DataElement::show
     */
    void show(const int level, const unsigned int indent) const override;

    /**
     * @brief Push an incoming data value into the DataElement.
     *
     * Called from the OPC UA client worker thread when new data is
     * received from the OPC UA session.
     *
     * @param value  new value for this data element
     * @param reason  reason for this value update
     */
    void setIncomingData(const UaVariant &value, ProcessReason reason);

    /**
     * @brief Push an incoming event into the DataElement.
     *
     * Called from the OPC UA client worker thread when an out-of-band
     * event was received (connection loss).
     *
     * @param reason  reason for this value update
     */
    void setIncomingEvent(ProcessReason reason);

    /**
     * @brief Get the outgoing data value from the DataElement.
     *
     * Called from the OPC UA client worker thread when data is being
     * assembled in OPC UA session for sending.
     *
     * @return  reference to outgoing data
     */
    const UaVariant &getOutgoingData() { return outgoingData; }

    /**
     * @brief Read incoming data as Int32. See DevOpcua::DataElement::readInt32
     *
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return value as epicsInt32
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsInt32 readInt32(ProcessReason *nextReason = nullptr,
                                 epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as Int64. See DevOpcua::DataElement::readInt64
     *
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return value as epicsInt64
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsInt64 readInt64(ProcessReason *nextReason = nullptr,
                                 epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as UInt32. See DevOpcua::DataElement::readUInt32
     *
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return value as epicsUInt32
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readUInt32(ProcessReason *nextReason = nullptr,
                                   epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as Float64. See DevOpcua::DataElement::readFloat64
     *
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return value as epicsFloat64
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsFloat64 readFloat64(ProcessReason *nextReason = nullptr,
                                     epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as classic C string (char[]).
     * See DevOpcua::DataElement::readCString
     *
     * @param value  pointer to target string buffer
     * @param num  max no. of bytes to copy (incl. NULL byte)
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual void readCString(char *value, const size_t num,
                             ProcessReason *nextReason = nullptr,
                             epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of Int8. See DevOpcua::DataElement::readArrayInt8
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayInt8(epicsInt8 *value, epicsUInt32 num,
                                      ProcessReason *nextReason = nullptr,
                                      epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of UInt8. See DevOpcua::DataElement::readArrayUInt8
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayUInt8(epicsUInt8 *value, epicsUInt32 num,
                                       ProcessReason *nextReason = nullptr,
                                       epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of Int16. See DevOpcua::DataElement::readArrayInt16
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayInt16(epicsInt16 *value, epicsUInt32 num,
                                       ProcessReason *nextReason = nullptr,
                                       epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of UInt16. See DevOpcua::DataElement::readArrayUInt16
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayUInt16(epicsUInt16 *value, epicsUInt32 num,
                                        ProcessReason *nextReason = nullptr,
                                        epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of Int32. See DevOpcua::DataElement::readArrayInt32
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayInt32(epicsInt32 *value, epicsUInt32 num,
                                       ProcessReason *nextReason = nullptr,
                                       epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of UInt32. See DevOpcua::DataElement::readArrayUInt32
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayUInt32(epicsUInt32 *value, epicsUInt32 num,
                                        ProcessReason *nextReason = nullptr,
                                        epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of Int64. See DevOpcua::DataElement::readArrayInt64
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayInt64(epicsInt64 *value, epicsUInt32 num,
                                       ProcessReason *nextReason = nullptr,
                                       epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of UInt64. See DevOpcua::DataElement::readArrayUInt64
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayUInt64(epicsUInt64 *value, epicsUInt32 num,
                                        ProcessReason *nextReason = nullptr,
                                        epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of Float32. See DevOpcua::DataElement::readArrayFloat32
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayFloat32(epicsFloat32 *value, epicsUInt32 num,
                                         ProcessReason *nextReason = nullptr,
                                         epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of Float64. See DevOpcua::DataElement::readArrayFloat64
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayFloat64(epicsFloat64 *value, epicsUInt32 num,
                                         ProcessReason *nextReason = nullptr,
                                         epicsTimeStamp *ts = nullptr) override;

    /**
     * @brief Read incoming data as array of EPICS Old String (fixed size).
     * See DevOpcua::DataElement::readArrayEpicsOldString
     *
     * @param value  pointer to target array
     * @param num  target array size
     * @param[out] nextReason  ProcessReason for the next update in the queue, `none` if last element
     * @param[out] ts  set to the EPICS time stamp of the update
     *
     * @return  number of elements written
     *
     * @throws std::runtime_error if no data present or on conversion error
     */
    virtual epicsUInt32 readArrayOldString(epicsOldString *value, epicsUInt32 num,
                                           ProcessReason *nextReason = nullptr,
                                           epicsTimeStamp *ts = nullptr) override;

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
     * @brief Write outgoing Int64 data. See DevOpcua::DataElement::writeInt64
     *
     * @param value  value to write
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeInt64(const epicsInt64 &value) override;

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
     * @brief Write outgoing array of Int8 data.
     * See DevOpcua::DataElement::writeArrayInt8
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayInt8(const epicsInt8 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of UInt8 data.
     * See DevOpcua::DataElement::writeArrayUInt8
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayUInt8(const epicsUInt8 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of Int16 data.
     * See DevOpcua::DataElement::writeArrayInt16
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayInt16(const epicsInt16 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of UInt16 data.
     * See DevOpcua::DataElement::writeArrayUInt16
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayUInt16(const epicsUInt16 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of Int32 data.
     * See DevOpcua::DataElement::writeArrayInt32
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayInt32(const epicsInt32 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of UInt32 data.
     * See DevOpcua::DataElement::writeArrayUInt32
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayUInt32(const epicsUInt32 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of Int64 data.
     * See DevOpcua::DataElement::writeArrayInt64
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayInt64(const epicsInt64 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of UInt64 data.
     * See DevOpcua::DataElement::writeArrayUInt64
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayUInt64(const epicsUInt64 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of Float32 data.
     * See DevOpcua::DataElement::writeArrayFloat32
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayFloat32(const epicsFloat32 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of Float64 data.
     * See DevOpcua::DataElement::writeArrayFloat64
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayFloat64(const epicsFloat64 *value, const epicsUInt32 num) override;

    /**
     * @brief Write outgoing array of OldString (40 char fixed size) data.
     *
     * @param value  pointer to source array
     * @param num  source array size
     *
     * @throws std::runtime_error on conversion error
     */
    virtual void writeArrayOldString(const epicsOldString *value, const epicsUInt32 num) override;

    /**
     * @brief Check status of last write service. See DevOpcua::DataElement::writeOk
     *
     * @return true = last write service ok
     */
    virtual bool writeWasOk() const override;

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

    /**
     * @brief Get debug level from record (via RecordConnector)`.
     * @return debug level
     */
    int debug() const { return (isLeaf() ? pconnector->debug() : pitem->debug()); }

private:
    void logWriteScalar () const;
    void checkIncomingEmpty() const;
    void logReadScalar(const Update<UaVariant> *upd, const std::string &targetTypeName) const;
    void checkReadArray(const Update<UaVariant> *upd,
                        OpcUa_BuiltInType expectedType, const epicsUInt32 targetSize, const std::string &targetTypeName) const;
    void checkWriteArray(OpcUa_BuiltInType expectedType, const std::string &targetTypeName) const;
    void logWriteArray(const epicsUInt32 targetSize, const std::string &targetTypeName) const;

    // Get the time stamp from the incoming object
    const epicsTime getIncomingTimeStamp() const {
        if (isLeaf())
            if (pconnector->plinkinfo->useServerTimestamp)
                return pitem->tsServer;
            else
                return pitem->tsSource;
        else
            return pitem->tsServer;
    }

    ItemUaSdk *pitem;                                       /**< corresponding item */
    std::vector<std::weak_ptr<DataElementUaSdk>> elements;  /**< children (if node) */
    std::shared_ptr<DataElementUaSdk> parent;               /**< parent */

    std::unordered_map<int,std::weak_ptr<DataElementUaSdk>> elementMap;

    bool mapped;                           /**< child name to index mapping done */
    UpdateQueue<UaVariant> incomingData;   /**< incoming values */
    OpcUa_BuiltInType incomingType;        /**< type of incoming data */
    bool incomingIsArray;                  /**< array property of incoming data */
    UaVariant outgoingData;                /**< outgoing value */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTUASDK_H
