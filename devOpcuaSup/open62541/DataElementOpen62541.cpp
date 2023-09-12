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

#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <cstdlib>

#include <errlog.h>
#include <epicsTime.h>
#include <alarm.h>

#include "ItemOpen62541.h"
#include "DataElementOpen62541.h"
#include "UpdateQueue.h"
#include "RecordConnector.h"

namespace DevOpcua {

/* Specific implementation of DataElement's "factory" method */
void
DataElement::addElementToTree(Item *item,
                              RecordConnector *pconnector,
                              const std::list<std::string> &elementPath)
{
    DataElementOpen62541::addElementToTree(static_cast<ItemOpen62541*>(item), pconnector, elementPath);
}

DataElementOpen62541::DataElementOpen62541 (const std::string &name,
                                    ItemOpen62541 *item,
                                    RecordConnector *pconnector)
    : DataElement(pconnector, name)
    , pitem(item)
    , timesrc(-1)
    , mapped(false)
    , incomingQueue(pconnector->plinkinfo->clientQueueSize, pconnector->plinkinfo->discardOldest)
    , outgoingLock(pitem->dataTreeWriteLock)
    , isdirty(false)
{
    UA_Variant_init(&incomingData);
    UA_Variant_init(&outgoingData);
}

DataElementOpen62541::DataElementOpen62541 (const std::string &name,
                                            ItemOpen62541 *item)
    : DataElement(name)
    , pitem(item)
    , timesrc(-1)
    , mapped(false)
    , incomingQueue(0ul)
    , outgoingLock(pitem->dataTreeWriteLock)
    , isdirty(false)
{
    UA_Variant_init(&incomingData);
    UA_Variant_init(&outgoingData);
}

void
DataElementOpen62541::addElementToTree(ItemOpen62541 *item,
                                       RecordConnector *pconnector,
                                       const std::list<std::string> &elementPath)
{
    std::string name("[ROOT]");
    if (elementPath.size())
        name = elementPath.back();

    auto leaf = std::make_shared<DataElementOpen62541>(name, item, pconnector);
    item->dataTree.addLeaf(leaf, elementPath);
    // reference from connector after adding to the tree worked
    pconnector->setDataElement(leaf);
}

void
DataElementOpen62541::show (const int level, const unsigned int indent) const
{
    std::string ind(static_cast<epicsInt64>(indent)*2, ' '); // static_cast to avoid warning C26451
    std::cout << ind;
    if (isLeaf()) {
        std::cout << "leaf=" << name << " record(" << pconnector->getRecordType() << ")="
                  << pconnector->getRecordName()
                  << " type=" << variantTypeString(incomingData)
                  << " timestamp=" << linkOptionTimestampString(pconnector->plinkinfo->timestamp)
                  << " bini=" << linkOptionBiniString(pconnector->plinkinfo->bini)
                  << " monitor=" << (pconnector->plinkinfo->monitor ? "y" : "n") << "\n";
    } else {
        std::cout << "node=" << name << " children=" << elements.size()
                  << " mapped=" << (mapped ? "y" : "n") << "\n";
        for (auto it : elements) {
            if (auto pelem = it.lock()) {
                pelem->show(level, indent + 1);
            }
        }
    }
}

bool
DataElementOpen62541::createMap (const UA_DataType *type,
                                 const std::string *timefrom)
{
    switch (typeKindOf(type)) {
    case UA_DATATYPEKIND_STRUCTURE:
    {
        if (debug() >= 5)
            std::cout << " ** creating index-to-element map for child elements" << std::endl;

        // Walk the structure and for each member cache
        // offset, array size (0 for scalars), and type
        // See copyStructure() in open62541 src/ua_types.c
        ptrdiff_t memberOffs = 0;
        for (unsigned int i = 0; i < type->membersSize; ++i) {
            const UA_DataTypeMember *member = &type->members[i];
#ifdef UA_BUILTIN_TYPES_COUNT
// Older open62541 before version 1.3 uses type index
            const UA_DataType *typelists[2] = { UA_TYPES, &type[-type->typeIndex] };
            const UA_DataType *memberType = &typelists[!member->namespaceZero][member->memberTypeIndex];
#else
// Newer open62541 before version 3.x uses pointer
            const UA_DataType *memberType = member->memberType;
#endif
            memberOffs += member->padding;
            if (member->isArray) {
                memberOffs += sizeof(size_t); // array length
                elementDesc.push_back({memberOffs, memberType});
                memberOffs += sizeof(void*);  // array data pointer
            } else {
                if (timefrom) {
#ifdef UA_ENABLE_TYPEDESCRIPTION
                    if (*timefrom == member->memberName) {
                        timesrc = static_cast<int>(memberOffs);
                    }
#endif
                }
                elementDesc.push_back({memberOffs, memberType});
                memberOffs += memberType->memSize;
            }
        }
        if (timefrom && timesrc == -1) {
            errlogPrintf(
                "%s: timestamp element %s not found - using source timestamp\n",
                pitem->recConnector->getRecordName(),
                timefrom->c_str());
        }

        // Map member names to index
        for (auto &it : elements) {
            auto pelem = it.lock();
            unsigned int i;
            for (i = 0; i < type->membersSize; i++) {
                if (pelem->name == type->members[i].memberName) {
                    elementMap.insert({i, it});
                    break;
                }
            }
            if (i == type->membersSize) {
                std::cerr << "Item " << pitem
                          << ": element " << pelem->name
                          << " not found in " << variantTypeString(type)
                          << std::endl;
            }
        }
        if (debug() >= 5)
            std::cout << " ** " << elementMap.size() << "/" << elements.size()
                      << " child elements mapped to a "
                      << "structure of " << type->membersSize << " elements" << std::endl;
        break;
    }
    case UA_TYPES_LOCALIZEDTEXT:
    {
        elementDesc = {
            {0, &UA_TYPES[UA_TYPES_STRING]},                 // Locale
            {sizeof(UA_String), &UA_TYPES[UA_TYPES_STRING]}  // Text
        };
        for (auto &it : elements) {
            auto pelem = it.lock();
            if (pelem->name == "Locale" || pelem->name == "locale") {
                elementMap.insert({0, it});
            } else if (pelem->name == "Text" || pelem->name == "text") {
                elementMap.insert({1, it});
            } else {
                 std::cerr << "Item " << pitem
                           << ": element " << pelem->name
                           << " not found in " << variantTypeString(type)
                           << std::endl;
            }
        }
        break;
    }
    default:
        std::cerr << "Item " << pitem
                  << " has unimplemented type " << variantTypeString(type)
                  << std::endl;
        return false;
    }
    mapped = true;
    return true;
}

// Getting the timestamp and status information from the Item assumes that only one thread
// is pushing data into the Item's DataElement structure at any time.
void
DataElementOpen62541::setIncomingData (const UA_Variant &value,
                                       ProcessReason reason,
                                       const std::string *timefrom)
{
    // Make a copy of this element and cache it

    UA_Variant_clear(&incomingData);
    UA_Variant_copy(&value, &incomingData);

    if (isLeaf()) {
        if ((pitem->state() == ConnectionStatus::initialRead
             && (reason == ProcessReason::readComplete || reason == ProcessReason::readFailure))
            || (pitem->state() == ConnectionStatus::up)) {

            Guard(pconnector->lock);
            bool wasFirst = false;
            // Make a copy of the value for this element and put it on the queue
            UA_Variant *valuecopy (new UA_Variant);
            UA_Variant_copy(&value, valuecopy); // As a non-C++ object, UA_Variant has no copy constructor
            UpdateOpen62541 *u(new UpdateOpen62541(getIncomingTimeStamp(), reason, std::unique_ptr<UA_Variant>(valuecopy), getIncomingReadStatus()));
            incomingQueue.pushUpdate(std::shared_ptr<UpdateOpen62541>(u), &wasFirst);
            if (debug() >= 5)
                std::cout << "Item " << pitem
                          << " element " << name
                          << " set data (" << processReasonString(reason)
                          << ") for record " << pconnector->getRecordName()
                          << " (queue use " << incomingQueue.size()
                          << "/" << incomingQueue.capacity() << ")" << std::endl;
            if (wasFirst)
                pconnector->requestRecordProcessing(reason);
        }
    } else {
        if (UA_Variant_isEmpty(&value))
            return;

        std::cerr << "Structured data in item " << pitem << " is not supported - ignoring" << std::endl;
        return;

        if (debug() >= 5)
            std::cout << "Item " << pitem
                      << " element " << name
                      << " splitting structured data to "
                      << elements.size() << " child elements"
                      << std::endl;

        const UA_DataType *type = value.type;
        char* container = static_cast<char*>(value.data);
        if (type->typeKind == UA_TYPES_EXTENSIONOBJECT) {
            UA_ExtensionObject &extensionObject = *reinterpret_cast<UA_ExtensionObject *>(container);
            if (extensionObject.encoding >= UA_EXTENSIONOBJECT_DECODED) {
                // Access content of decoded extension objects
                type = extensionObject.content.decoded.type;
                container = static_cast<char*>(extensionObject.content.decoded.data);
            } else {
                std::cerr << "Item " << pitem << " is not decoded" << std::endl;
                return;
            }
        }
        if (!mapped) {
            createMap(type, timefrom);
        }
        for (auto &it : elementMap) {
            auto pelem = it.second.lock();
            int index = it.first;
            const ElementDesc& ed = elementDesc[index];
            void* memberData = container + ed.offs;
            UA_Variant value;
            if (ed.type->members && ed.type->members[index].isArray)
            {
                size_t arrayLength = static_cast<size_t*>(memberData)[-1];
                if (arrayLength == 0) memberData = UA_EMPTY_ARRAY_SENTINEL;
                UA_Variant_setArray(&value, memberData, arrayLength, ed.type);
            } else {
                UA_Variant_setScalar(&value, memberData, ed.type);
            }
            pelem->setIncomingData(value, reason);
        }
    }
}

void
DataElementOpen62541::setIncomingEvent (ProcessReason reason)
{
    if (isLeaf()) {
        Guard(pconnector->lock);
        bool wasFirst = false;
        // Put the event on the queue
        UpdateOpen62541 *u(new UpdateOpen62541(getIncomingTimeStamp(), reason));
        incomingQueue.pushUpdate(std::shared_ptr<UpdateOpen62541>(u), &wasFirst);
        if (debug() >= 5)
            std::cout << "Element " << name << " set event ("
                      << processReasonString(reason)
                      << ") for record " << pconnector->getRecordName()
                      << " (queue use " << incomingQueue.size()
                      << "/" << incomingQueue.capacity() << ")"
                      << std::endl;
        if (wasFirst)
            pconnector->requestRecordProcessing(reason);
    } else {
        for (auto &it : elements) {
            auto pelem = it.lock();
            pelem->setIncomingEvent(reason);
        }
    }
}

// Helper to update one data structure element from pointer to child
bool
DataElementOpen62541::updateDataInStruct (void* container,
                                          const int index,
                                          std::shared_ptr<DataElementOpen62541> pelem)
{
    bool updated = false;
    { // Scope of Guard G
        Guard G(pelem->outgoingLock);
        if (pelem->isDirty()) {
            const ElementDesc& ed = elementDesc[index];
            void* memberData = static_cast<char*>(container) + ed.offs;
            const UA_Variant& data = pelem->getOutgoingData();
            assert(ed.type == data.type);
            if (ed.type->members && ed.type->members[index].isArray)
            {
                size_t &arrayLength = static_cast<size_t*>(memberData)[-1];
                UA_Array_delete(memberData, arrayLength, ed.type);
                UA_StatusCode status = UA_Array_copy(data.data, data.arrayLength, &memberData, ed.type);
                if (status == UA_STATUSCODE_GOOD) {
                    arrayLength = data.arrayLength;
                } else {
                    arrayLength = 0;
                    std::cerr << "Item " << pitem
                              << ": inserting data from from child element" << pelem->name
                              << " failed (" << UA_StatusCode_name(status) << ')'
                              << std::endl;
                    return false;
                }
            } else {
                UA_copy(data.data, memberData, ed.type);
            }
            pelem->isdirty = false;
            updated = true;
        }
    }
    if (debug() >= 4) {
        if (updated) {
            std::cout << "Data from child element " << pelem->name
                      << " inserted into data structure" << std::endl;
        } else {
            std::cout << "Data from child element " << pelem->name
                      << " ignored (not dirty)" << std::endl;
        }
    }
    return updated;
}


const UA_Variant &
DataElementOpen62541::getOutgoingData ()
{
    if (!isLeaf()) {

        std::cerr << "Structured data in item " << pitem
                  << " element " << name << " is not supported - ignoring" << std::endl;
        return outgoingData;

        if (debug() >= 4)
            std::cout << "Item " << pitem
                      << " element " << name
                      << " updating structured data from "
                      << elements.size() << " child elements"
                      << std::endl;

        UA_Variant_clear(&outgoingData);
        UA_Variant_copy(&incomingData, &outgoingData);
        isdirty = false;
        const UA_DataType *type = outgoingData.type;
        void* container = outgoingData.data;
        if (type && type->typeKind == UA_TYPES_EXTENSIONOBJECT) {
            UA_ExtensionObject &extensionObject = *reinterpret_cast<UA_ExtensionObject *>(container);
            if (extensionObject.encoding >= UA_EXTENSIONOBJECT_DECODED) {
                // Access content decoded extension objects
                type = extensionObject.content.decoded.type;
                container = extensionObject.content.decoded.data;
            } else {
                std::cerr << "Item " << pitem << " is not decoded" << std::endl;
                return outgoingData;
            }
        }
        if (!mapped) {
            createMap(type, nullptr);
        }
        for (auto &it : elementMap) {
            auto pelem = it.second.lock();
            if (updateDataInStruct(container, it.first, pelem))
               isdirty = true;
        }
        if (isdirty) {
            if (debug() >= 4)
                std::cout << "Encoding changed data structure to outgoingData of element " << name
                          << std::endl;
        } else {
            if (debug() >= 4)
                std::cout << "Returning unchanged outgoingData of element " << name
                          << std::endl;
        }
    }
    return outgoingData;
}

void
DataElementOpen62541::dbgReadScalar (const UpdateOpen62541 *upd,
                                 const std::string &targetTypeName,
                                 const size_t targetSize) const
{
    if (isLeaf() && debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        ProcessReason reason = upd->getType();

        std::cout << pconnector->getRecordName() << ": ";
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete) {
            UA_Variant &data = upd->getData();
            std::cout << "(" << linkOptionTimestampString(pconnector->plinkinfo->timestamp);
            if (pconnector->plinkinfo->timestamp == LinkOptionTimestamp::data)
                std::cout << "(@" << pconnector->plinkinfo->timestampElement << ")";
            std::cout << " time " << time_buf << ") read " << processReasonString(reason) << " ("
                      << UA_StatusCode_name(upd->getStatus()) << ") "
                      << data
                      << " as " << targetTypeName;
            if (targetSize)
                std::cout << "[" << targetSize << "]";
        } else {
            std::cout << "(client time "<< time_buf << ") " << processReasonString(reason);
        }
        std::cout << " --- remaining queue " << incomingQueue.size()
                  << "/" << incomingQueue.capacity() << std::endl;
    }
}

long
DataElementOpen62541::readScalar (epicsInt32 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    return readScalar<epicsInt32>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readScalar (epicsInt64 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    return readScalar<epicsInt64>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readScalar (epicsUInt32 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    return readScalar<epicsUInt32>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readScalar (epicsFloat64 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    return readScalar<epicsFloat64>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

// CString type needs specialization
long
DataElementOpen62541::readScalar (char *value, const size_t num,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    long ret = 0;

    if (incomingQueue.empty()) {
        errlogPrintf("%s: incoming data queue empty\n", prec->name);
        if (nextReason)
            *nextReason = ProcessReason::none;
        return 1;
    }

    ProcessReason nReason;
    std::shared_ptr<UpdateOpen62541> upd = incomingQueue.popUpdate(&nReason);
    dbgReadScalar(upd.get(), "CString", num);

    switch (upd->getType()) {
    case ProcessReason::readFailure:
        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        ret = 1;
        break;
    case ProcessReason::connectionLoss:
        (void) recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM);
        ret = 1;
        break;
    case ProcessReason::incomingData:
    case ProcessReason::readComplete:
    {
        if (num && value) {
            UA_StatusCode stat = upd->getStatus();
            if (UA_STATUS_IS_BAD(stat)) {
                // No valid OPC UA value
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                // Valid OPC UA value, so copy over
                if (UA_STATUS_IS_UNCERTAIN(stat)) {
                    (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                }
                UA_Variant &data = upd->getData();
                size_t n = num-1;
                if (typeKindOf(data) == UA_TYPES_STRING) {
                    UA_String *datastring = static_cast<UA_String *>(data.data); // Not terminated!
                    if (n > datastring->length)
                        n = datastring->length;
                    strncpy(value, reinterpret_cast<char*>(datastring->data), n);
                } else {
                    UA_String datastring = UA_STRING_NULL;
                    if (data.type)
                        UA_print(data.data, data.type, &datastring); // Not terminated!
                    if (n > datastring.length)
                        n = datastring.length;
                    strncpy(value, reinterpret_cast<char*>(datastring.data), n);
                    UA_String_clear(&datastring);
                }
                value[n] = '\0';
                prec->udf = false;
                UA_Variant_clear(&data);
            }
            if (statusCode) *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UA_StatusCode_name(stat), statusTextLen);
                statusText[statusTextLen-1] = '\0';
            }
        }
        break;
    }
    default:
        break;
    }

    prec->time = upd->getTimeStamp();
    if (nextReason) *nextReason = nReason;
    return ret;
}

void
DataElementOpen62541::dbgReadArray (const UpdateOpen62541 *upd,
                                const epicsUInt32 targetSize,
                                const std::string &targetTypeName) const
{
    if (isLeaf() && debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        ProcessReason reason = upd->getType();

        std::cout << pconnector->getRecordName() << ": ";
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete) {
            std::cout << "(" << linkOptionTimestampString(pconnector->plinkinfo->timestamp);
            if (pconnector->plinkinfo->timestamp == LinkOptionTimestamp::data)
                std::cout << "@" << pconnector->plinkinfo->timestampElement;
            std::cout << " time " << time_buf << ") read " << processReasonString(reason) << " ("
                      << UA_StatusCode_name(upd->getStatus()) << ") ";
            UA_Variant &data = upd->getData();
            std::cout << " array of " << variantTypeString(data)
                      << "[" << upd->getData().arrayLength << "]"
                      << " into " << targetTypeName << "[" << targetSize << "]";
        } else {
            std::cout << "(client time "<< time_buf << ") " << processReasonString(reason);
        }
        std::cout << " --- remaining queue " << incomingQueue.size()
                  << "/" << incomingQueue.capacity() << std::endl;
    }
}

// Read array for EPICS String / UA_String
long int
DataElementOpen62541::readArray (char *value, const epicsUInt32 len,
                             const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             const UA_DataType *expectedType,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    long ret = 0;
    epicsUInt32 elemsWritten = 0;

    if (incomingQueue.empty()) {
        errlogPrintf("%s : incoming data queue empty\n", prec->name);
        *numRead = 0;
        return 1;
    }

    ProcessReason nReason;
    std::shared_ptr<UpdateOpen62541> upd = incomingQueue.popUpdate(&nReason);
    dbgReadArray(upd.get(), num, epicsTypeString(value));

    switch (upd->getType()) {
    case ProcessReason::readFailure:
        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        ret = 1;
        break;
    case ProcessReason::connectionLoss:
        (void) recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM);
        ret = 1;
        break;
    case ProcessReason::incomingData:
    case ProcessReason::readComplete:
    {
        if (num && value) {
            UA_StatusCode stat = upd->getStatus();
            if (UA_STATUS_IS_BAD(stat)) {
                // No valid OPC UA value
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                // Valid OPC UA value, so try to convert
                UA_Variant &data = upd->getData();
                if (UA_Variant_isScalar(&data)) {
                    errlogPrintf("%s : incoming data is not an array\n", prec->name);
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else if (data.type != expectedType) {
                    errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                 prec->name, variantTypeString(data), epicsTypeString(value));
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    if (UA_STATUS_IS_UNCERTAIN(stat)) {
                        (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                    }
                    elemsWritten = static_cast<epicsUInt32>(num < data.arrayLength ? num : data.arrayLength);
                    for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                        strncpy(value + i * len, reinterpret_cast<char*>(static_cast<UA_String *>(data.data)[i].data), len);
                        (value + i * len)[len - 1] = '\0';
                    }
                    prec->udf = false;
                }
                UA_Variant_clear(&data);
            }
            if (statusCode) *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UA_StatusCode_name(stat), statusTextLen);
                statusText[statusTextLen-1] = '\0';
            }
        }
        break;
    }
    default:
        break;
    }

    prec->time = upd->getTimeStamp();
    if (nextReason) *nextReason = nReason;
    if (num && value)
        *numRead = elemsWritten;
    return ret;
}

long
DataElementOpen62541::readArray (epicsInt8 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt8>(value, num, numRead, &UA_TYPES[UA_TYPES_SBYTE], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsUInt8 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt8>(value, num, numRead, &UA_TYPES[UA_TYPES_BYTE], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsInt16 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt16>(value, num, numRead, &UA_TYPES[UA_TYPES_INT16], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsUInt16 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt16>(value, num, numRead, &UA_TYPES[UA_TYPES_UINT16], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsInt32 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt32>(value, num, numRead, &UA_TYPES[UA_TYPES_INT32], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsUInt32 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt32>(value, num, numRead, &UA_TYPES[UA_TYPES_UINT32], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsInt64 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt64>(value, num, numRead, &UA_TYPES[UA_TYPES_INT64], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsUInt64 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt64>(value, num, numRead, &UA_TYPES[UA_TYPES_UINT64], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsFloat32 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsFloat32>(value, num, numRead, &UA_TYPES[UA_TYPES_FLOAT], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsFloat64 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsFloat64>(value, num, numRead, &UA_TYPES[UA_TYPES_DOUBLE], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (char *value, const epicsUInt32 len,
                             const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray(value, len, num, numRead, &UA_TYPES[UA_TYPES_STRING], prec, nextReason, statusCode, statusText, statusTextLen);
}

inline
void
DataElementOpen62541::dbgWriteScalar () const
{
    if (isLeaf() && debug()) {
        std::cout << pconnector->getRecordName()
                  << ": set outgoing data to value "
                  << outgoingData << std::endl;
    }
}

long
DataElementOpen62541::writeScalar (const epicsInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsInt32>(value, prec);
}

long
DataElementOpen62541::writeScalar (const epicsUInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsUInt32>(value, prec);
}

long
DataElementOpen62541::writeScalar (const epicsInt64 &value, dbCommon *prec)
{
    return writeScalar<epicsInt64>(value, prec);
}

long
DataElementOpen62541::writeScalar (const epicsFloat64 &value, dbCommon *prec)
{
    return writeScalar<epicsFloat64>(value, prec);
}

long
DataElementOpen62541::writeScalar (const char *value, const epicsUInt32 len, dbCommon *prec)
{
    long ret = 0;
    UA_StatusCode status = UA_STATUSCODE_BADUNEXPECTEDERROR;
    long l;
    unsigned long ul;
    double d;

    switch (typeKindOf(incomingData)) {
    case UA_TYPES_STRING:
    {
        UA_String val;
        val.length = strlen(value);
        val.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(value));
        { // Scope of Guard G
            Guard G(outgoingLock);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_STRING]);
            markAsDirty();
        }
        break;
    }
    case UA_TYPES_BOOLEAN:
    { // Scope of Guard G
        Guard G(outgoingLock);
        UA_Boolean val = strchr("YyTt1", *value) != NULL;
        status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_BOOLEAN]);
        markAsDirty();
        break;
    }
    case UA_TYPES_BYTE:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<UA_Byte>(ul)) {
            Guard G(outgoingLock);
            UA_Byte val = static_cast<UA_Byte>(ul);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_BYTE]);
            markAsDirty();
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_SBYTE:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<UA_SByte>(l)) {
            Guard G(outgoingLock);
            UA_SByte val = static_cast<UA_Byte>(l);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_SBYTE]);
            markAsDirty();
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_UINT16:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<UA_UInt16>(ul)) {
            Guard G(outgoingLock);
            UA_UInt16 val = static_cast<UA_UInt16>(ul);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT16]);
            markAsDirty();
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_INT16:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<UA_Int16>(l)) {
            Guard G(outgoingLock);
            UA_Int16 val = static_cast<UA_Int16>(l);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT16]);
            markAsDirty();
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_UINT32:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<UA_UInt32>(ul)) {
            Guard G(outgoingLock);
            UA_UInt32 val = static_cast<UA_UInt32>(ul);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT32]);
            markAsDirty();
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_INT32:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<UA_Int32>(l)) {
            Guard G(outgoingLock);
            UA_Int32 val = static_cast<UA_Int32>(l);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT32]);
            markAsDirty();
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_UINT64:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<UA_UInt64>(ul)) {
            Guard G(outgoingLock);
            UA_UInt64 val = static_cast<UA_UInt64>(ul);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT64]);
            markAsDirty();
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_INT64:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<UA_Int64>(l)) {
            Guard G(outgoingLock);
            UA_Int64 val = static_cast<UA_Int64>(l);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT64]);
            markAsDirty();
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_FLOAT:
        d = strtod(value, nullptr);
        if (isWithinRange<UA_Float>(d)) {
            Guard G(outgoingLock);
            UA_Float val = static_cast<UA_Float>(d);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_FLOAT]);
            markAsDirty();
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_DOUBLE:
    {
        d = strtod(value, nullptr);
        Guard G(outgoingLock);
        UA_Double val = static_cast<UA_Double>(d);
        status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_DOUBLE]);
        markAsDirty();
        break;
    }
    default:
        errlogPrintf("%s : unsupported conversion for outgoing data\n",
                     prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
    }
    if (ret == 0 && UA_STATUS_IS_BAD(status)) {
        errlogPrintf("%s : scalar copy failed: %s\n",
                     prec->name, UA_StatusCode_name(status));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    }
    dbgWriteScalar();
    return ret;
}

inline
void
DataElementOpen62541::dbgWriteArray (const epicsUInt32 targetSize, const std::string &targetTypeName) const
{
    if (isLeaf() && debug()) {
        std::cout << pconnector->getRecordName() << ": writing array of "
                  << targetTypeName << "[" << targetSize << "] as "
                  << variantTypeString(outgoingData) << "["<< outgoingData.arrayLength << "]"
                  << std::endl;
    }
}

// Write array for EPICS String / UA_String
long
DataElementOpen62541::writeArray (const char **value, const epicsUInt32 len,
                              const epicsUInt32 num,
                              const UA_DataType *targetType,
                              dbCommon *prec)
{
    long ret = 0;

    if (UA_Variant_isScalar(&incomingData)) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else if (incomingData.type != targetType) {
        errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                     prec->name,
                     variantTypeString(incomingData),
                     variantTypeString(targetType),
                     epicsTypeString(*value));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
        UA_String *arr = static_cast<UA_String *>(UA_Array_new(num, &UA_TYPES[UA_TYPES_STRING]));
        if (!arr) {
            errlogPrintf("%s : out of memory\n", prec->name);
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        } else {
            for (epicsUInt32 i = 0; i < num; i++) {
                char *val = nullptr;
                const char *pval;
                // add zero termination if necessary
                if (memchr(value[i], '\0', len) == nullptr) {
                    val = new char[static_cast<epicsInt64>(len)+1]; // static_cast to avoid warning C26451
                    strncpy(val, value[i], len);
                    val[len] = '\0';
                    pval = val;
                } else {
                    pval = value[i];
                }
                arr[i] = UA_STRING_ALLOC(pval);
                delete[] val;
            }
            UA_StatusCode status;
            { // Scope of Guard G
                Guard G(outgoingLock);
                status = UA_Variant_setArrayCopy(&outgoingData, arr, num, targetType);
                markAsDirty();
            }
            if (UA_STATUS_IS_BAD(status)) {
                errlogPrintf("%s : array copy failed: %s\n",
                             prec->name, UA_StatusCode_name(status));
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                dbgWriteArray(num, epicsTypeString(*value));
            }
        }
    }
    return ret;
}

long
DataElementOpen62541::writeArray (const epicsInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt8>(value, num, &UA_TYPES[UA_TYPES_SBYTE], prec);
}

long
DataElementOpen62541::writeArray (const epicsUInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt8>(value, num, &UA_TYPES[UA_TYPES_SBYTE], prec);
}

long
DataElementOpen62541::writeArray (const epicsInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt16>(value, num, &UA_TYPES[UA_TYPES_INT16], prec);
}

long
DataElementOpen62541::writeArray (const epicsUInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt16>(value, num, &UA_TYPES[UA_TYPES_UINT16], prec);
}

long
DataElementOpen62541::writeArray (const epicsInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt32>(value, num, &UA_TYPES[UA_TYPES_INT32], prec);
}

long
DataElementOpen62541::writeArray (const epicsUInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt32>(value, num, &UA_TYPES[UA_TYPES_UINT32], prec);
}

long
DataElementOpen62541::writeArray (const epicsInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt64>(value, num, &UA_TYPES[UA_TYPES_INT64], prec);
}

long
DataElementOpen62541::writeArray (const epicsUInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt64>(value, num, &UA_TYPES[UA_TYPES_UINT64], prec);
}

long
DataElementOpen62541::writeArray (const epicsFloat32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat32>(value, num, &UA_TYPES[UA_TYPES_FLOAT], prec);
}

long
DataElementOpen62541::writeArray (const epicsFloat64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat64>(value, num, &UA_TYPES[UA_TYPES_DOUBLE], prec);
}

long
DataElementOpen62541::writeArray (const char *value, const epicsUInt32 len, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray(&value, len, num, &UA_TYPES[UA_TYPES_STRING], prec);
}

void
DataElementOpen62541::requestRecordProcessing (const ProcessReason reason) const
{
    if (isLeaf()) {
        pconnector->requestRecordProcessing(reason);
    } else {
        for (auto &it : elementMap) {
            auto pelem = it.second.lock();
            pelem->requestRecordProcessing(reason);
        }
    }
}

} // namespace DevOpcua
