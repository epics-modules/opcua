/*************************************************************************\
* Copyright (c) 2018-2025 ITER Organization.
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

#include <memory>
#include <string>

#include <statuscode.h>
#include <uadatavalue.h>

#include <errlog.h>

#include "devOpcua.h"
#include "ItemUaSdk.h"

namespace DevOpcua {

class DataElementUaSdkNode;

inline const char *
variantTypeString (const OpcUa_BuiltInType type)
{
    switch(type) {
    case OpcUaType_Null:            return "OpcUa_Null";
    case OpcUaType_Boolean:         return "OpcUa_Boolean";
    case OpcUaType_SByte:           return "OpcUa_SByte";
    case OpcUaType_Byte:            return "OpcUa_Byte";
    case OpcUaType_Int16:           return "OpcUa_Int16";
    case OpcUaType_UInt16:          return "OpcUa_UInt16";
    case OpcUaType_Int32:           return "OpcUa_Int32";
    case OpcUaType_UInt32:          return "OpcUa_UInt32";
    case OpcUaType_Int64:           return "OpcUa_Int64";
    case OpcUaType_UInt64:          return "OpcUa_UInt64";
    case OpcUaType_Float:           return "OpcUa_Float";
    case OpcUaType_Double:          return "OpcUa_Double";
    case OpcUaType_String:          return "OpcUa_String";
    case OpcUaType_DateTime:        return "OpcUa_DateTime";
    case OpcUaType_Guid:            return "OpcUa_Guid";
    case OpcUaType_ByteString:      return "OpcUa_ByteString";
    case OpcUaType_XmlElement:      return "OpcUa_XmlElement";
    case OpcUaType_NodeId:          return "OpcUa_NodeId";
    case OpcUaType_ExpandedNodeId:  return "OpcUa_ExpandedNodeId";
    case OpcUaType_StatusCode:      return "OpcUa_StatusCode";
    case OpcUaType_QualifiedName:   return "OpcUa_QualifiedName";
    case OpcUaType_LocalizedText:   return "OpcUa_LocalizedText";
    case OpcUaType_ExtensionObject: return "OpcUa_ExtensionObject";
    case OpcUaType_DataValue:       return "OpcUa_DataValue";
    case OpcUaType_Variant:         return "OpcUa_Variant";
    case OpcUaType_DiagnosticInfo:  return "OpcUa_DiagnosticInfo";
    }
    return "Illegal Value";
}


/**
 * @brief The DataElementUaSdk implementation of a single piece of data.
 *
 * This class implements the common parts of a Node and a Leaf element.
 * See DevOpcua::DataElement
 *
 */
class DataElementUaSdk
{
    friend class DataElementUaSdkNode;
    friend class DataElementUaSdkLeaf;

public:
    DataElementUaSdk (const std::string &name, class ItemUaSdk *pitem)
        : name(name)
        , pitem(pitem)
        , outgoingLock(pitem->dataTreeWriteLock)
    {}
    virtual ~DataElementUaSdk();

    /* ElementTree interface */
    void setParent(std::shared_ptr<DataElementUaSdk> elem) { parent = elem; }
    virtual bool isLeaf() const = 0;
    virtual void addChild(std::weak_ptr<DataElementUaSdk> elem) = 0;
    virtual std::shared_ptr<DataElementUaSdk> findChild(const std::string &name) const = 0;

    /**
     * @brief Print configuration and status.
     * See DevOpcua::DataElement::show
     */
    virtual void show(const int level, const unsigned int indent) const = 0;

    /**
     * @brief Push an incoming data value into the DataElement.
     *
     * Called from the OPC UA client worker thread when new data is
     * received from the OPC UA session.
     *
     * @param value  new value for this data element
     * @param reason  reason for this value update
     * @param timefrom  name of element to read item timestamp from
     * @param typeId  data type of the data element
     */
    virtual void setIncomingData(const UaVariant &value,
                                 ProcessReason reason,
                                 const std::string *timefrom = nullptr,
                                 const UaNodeId *typeId = nullptr)
        = 0;

    /**
     * @brief Push an incoming event into the DataElement.
     *
     * Called from the OPC UA client worker thread when an out-of-band
     * event was received (connection loss).
     *
     * @param reason  reason for this value update
     */
    virtual void setIncomingEvent(ProcessReason reason) = 0;

    /**
     * @brief Set the EPICS-related state of the element and all sub-elements.
     */
    virtual void setState(const ConnectionStatus state) = 0;

    /**
     * @brief Get the outgoing data value from the DataElement.
     *
     * Called from the OPC UA client worker thread when data is being
     * assembled in OPC UA session for sending.
     *
     * @return  reference to outgoing data
     */
    virtual const UaVariant &getOutgoingData() = 0;

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
    virtual void clearOutgoingData() = 0;

    /**
     * @brief Create processing requests for record(s) attached to this element.
     * See DevOpcua::DataElement::requestRecordProcessing
     */
    virtual void requestRecordProcessing(const ProcessReason reason) const = 0;

    /**
     * @brief Get debug level from item or record (via RecordConnector).
     * @return debug level
     */
    virtual int debug() const { return pitem->debug(); }

    const std::string name;                       /**< element name */

private:
    virtual bool isDirty() const = 0;
    virtual void markAsDirty() = 0;

protected:
    ItemUaSdk *pitem;                             /**< corresponding item */
    std::shared_ptr<DataElementUaSdk> parent;     /**< parent */

    UaVariant incomingData;   /**< cache of latest incoming value */
    epicsMutex &outgoingLock; /**< data lock for outgoing value */
    UaVariant outgoingData;   /**< cache of latest outgoing value */
    bool isdirty;             /**< outgoing value has been (or needs to be) updated */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTUASDK_H
