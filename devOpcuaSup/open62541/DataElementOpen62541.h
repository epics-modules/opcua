/*************************************************************************\
* Copyright (c) 2018-2023 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#ifndef DEVOPCUA_DATAELEMENTOPEN62541_H
#define DEVOPCUA_DATAELEMENTOPEN62541_H

#include "Update.h"
#include "ItemOpen62541.h"

#include <open62541/client.h>
#ifndef UA_STATUSCODE_BAD  // Not yet defined in open62541 version 1.2
#define UA_STATUSCODE_BAD 0x80000000
#endif
#define UA_STATUS_IS_BAD(status) (((status)&UA_STATUSCODE_BAD)!=0)

#ifndef UA_STATUSCODE_UNCERTAIN
#define UA_STATUSCODE_UNCERTAIN 0x40000000
#endif
#define UA_STATUS_IS_UNCERTAIN(status) (((status)&UA_STATUSCODE_UNCERTAIN)!=0)

#include <string>

namespace DevOpcua {

typedef Update<UA_Variant, UA_StatusCode> UpdateOpen62541;

inline const char *
variantTypeString (const UA_DataType *type)
{
    return type ? type->typeName : "Null";
}

inline const char *
variantTypeString (const UA_Variant& v) {
    return variantTypeString(v.type);
}

inline int typeKindOf(const UA_DataType *type)
{
    return type ? (int)type->typeKind : -1;
}

inline int typeKindOf(const UA_Variant& v)
{
    return typeKindOf(v.type);
}

/**
 * @brief The DataElementOpen62541 implementation of a single piece of data.
 *
 * See DevOpcua::DataElement
 */
class DataElementOpen62541
{
    friend class DataElementOpen62541Node;
    friend class DataElementOpen62541Leaf;

public:
    DataElementOpen62541 (const std::string &name, class ItemOpen62541 *pitem)
        : name(name)
        , pitem(pitem)
        , outgoingLock(pitem->dataTreeWriteLock)
    {}
    virtual ~DataElementOpen62541();

    /* ElementTree node interface methods */
    void setParent (std::shared_ptr<DataElementOpen62541> elem) { parent = elem; }
    virtual bool isLeaf() const = 0;
    virtual void addChild(std::weak_ptr<DataElementOpen62541> elem) = 0;
    virtual std::shared_ptr<DataElementOpen62541> findChild(const std::string &name) const = 0;

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
     */
    virtual void setIncomingData(const UA_Variant &value, ProcessReason reason, const std::string *timefrom = nullptr)
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
    virtual const UA_Variant &getOutgoingData() = 0;


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
     * @brief Move the contents of the current outgoing data.
     *
     * Avoids a deep copy by moving the contents out of the outgoing data
     * and clearing it afterwards.
     *
     * Call holding outgoingLock!
     */
    void *moveOutgoingData ()
    {
        void* data = outgoingData.data;
        outgoingData.data = nullptr;
        UA_Variant_clear(&outgoingData);
        return data;
    }

    /**
     * @brief Create processing requests for record(s) attached to this element.
     * See DevOpcua::DataElement::requestRecordProcessing
     */
    virtual void requestRecordProcessing(const ProcessReason reason) const = 0;

    /**
     * @brief Get debug level from record (via RecordConnector).
     * @return debug level
     */
    virtual int debug() const { return pitem->debug(); }

    const std::string name;

private:
    virtual bool isDirty() const = 0;
    virtual void markAsDirty() = 0;

protected:
    ItemOpen62541 *pitem;                                       /**< corresponding item */
    std::shared_ptr<DataElementOpen62541> parent;               /**< parent */

    UA_Variant incomingData;                 /**< cache of latest incoming value */
    epicsMutex &outgoingLock;                /**< data lock for outgoing value */
    UA_Variant outgoingData;                 /**< cache of latest outgoing value */
    bool isdirty;                            /**< outgoing value has been (or needs to be) updated */

    const UA_DataType *memberType = nullptr; /**< type of this element */
    UA_Boolean isArray = false;              /**< is this element an array? */
    UA_Boolean isOptional = false;           /**< is this element optional? */
    size_t offset = 0;                       /**< data offset of this element in parent structure */
    UA_UInt32 index = 0;                     /**< element index (for unions) */

    friend std::ostream& operator << (std::ostream& os, const DataElementOpen62541& element);
};

// print the full element name, e.g.: item elem.array[index].elem
inline std::ostream& operator << (std::ostream& os, const DataElementOpen62541& element)
{
    // Skip the [ROOT] element in printing and print the item name instead
    if (!element.parent)
        return os << element.pitem;
    os << element.parent;
    if (!element.parent->parent)
        os << " element ";
    else if (element.name[0] != '[')
        os << '.';
    return os << element.name;
}

inline std::ostream& operator << (std::ostream& os, const DataElementOpen62541* pelement)
{
    return os << *pelement;
}

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTOPEN62541_H
