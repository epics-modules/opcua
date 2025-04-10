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

#define epicsExportSharedSymbols
#include "DataElementOpen62541.h"
#include "ItemOpen62541.h"

#include <errlog.h>
#include <epicsTypes.h>

#include <open62541/client.h>

#include <string>
#include <list>
#include <memory>
#include <iostream>

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

#ifndef UA_ENABLE_TYPEDESCRIPTION
// Without type description, we cannot get struct members
#error Set UA_ENABLE_TYPEDESCRIPTION in open62541
#endif

// Older open62541 version 1.2 has no UA_DataType_getStructMember
// Code from open62541 version 1.3.7 modified for compatibility with version 1.2
// and extended to return isOptional flag and index for union

inline const UA_DataType*
memberTypeOf(const UA_DataType *type, const UA_DataTypeMember *m) {
#ifdef UA_DATATYPES_USE_POINTER
    return m->memberType;
#else
    const UA_DataType *typelists[2] = { UA_TYPES, &type[-type->typeIndex] };
    return &typelists[!m->namespaceZero][m->memberTypeIndex];
#endif
}

UA_UInt32
UA_DataType_getStructMemberExt(const UA_DataType *type, const char *memberName,
                            size_t *outOffset, const UA_DataType **outMemberType,
                            UA_Boolean *outIsArray, UA_Boolean *outIsOptional) {

    size_t offset = 0;
    switch(type->typeKind) {
    case UA_DATATYPEKIND_STRUCTURE:
    case UA_DATATYPEKIND_OPTSTRUCT:
    case UA_DATATYPEKIND_UNION:
        for(UA_UInt32 i = 0; i < type->membersSize; ++i) {
            const UA_DataTypeMember *m = &type->members[i];
            const UA_DataType *mt = memberTypeOf(type, m);
            offset += m->padding;

            if(strcmp(memberName, m->memberName) == 0) {
                *outOffset = offset;
                *outMemberType = mt;
                *outIsArray = m->isArray;
                *outIsOptional = m->isOptional;
                return i+1;
            }

            if (type->typeKind == UA_DATATYPEKIND_UNION) {
                offset = 0;
            } else if(!m->isOptional) {
                if(!m->isArray) {
                    offset += mt->memSize;
                } else {
                    offset += sizeof(size_t);
                    offset += sizeof(void*);
                }
            } else { /* field is optional */
                if(!m->isArray) {
                    offset += sizeof(void *);
                } else {
                    offset += sizeof(size_t);
                    offset += sizeof(void *);
                }
            }
        }
        break;
    case UA_DATATYPEKIND_LOCALIZEDTEXT:
        *outMemberType = &UA_TYPES[UA_TYPES_STRING];
        *outIsArray = 0;
        *outIsOptional = 0;
        if (strcmp(memberName, "locale") == 0) {
            *outOffset = offsetof(UA_LocalizedText, locale);
            return 1;
        }
        if (strcmp(memberName, "text") == 0) {
            *outOffset = offsetof(UA_LocalizedText, text);
            return 2;
        }
        break;
    case UA_DATATYPEKIND_QUALIFIEDNAME:
        *outIsArray = 0;
        *outIsOptional = 0;
        if (strcmp(memberName, "namespaceIndex") == 0) {
            *outMemberType = &UA_TYPES[UA_DATATYPEKIND_UINT16];
            *outOffset = offsetof(UA_QualifiedName, namespaceIndex);
            return 1;
        }
        if (strcmp(memberName, "name") == 0) {
            *outMemberType = &UA_TYPES[UA_TYPES_STRING];
            *outOffset = offsetof(UA_QualifiedName, name);
            return 2;
        }
        break;
    }
    return 0;
}

void
DataElementOpen62541::createMap(const UA_DataType *type,
                                const std::string *timefrom)
{
    if (debug() >= 5)
        std::cout << " ** creating index-to-element map for child elements" << std::endl;

    switch (typeKindOf(type)) {
    case UA_DATATYPEKIND_STRUCTURE:
    case UA_DATATYPEKIND_OPTSTRUCT:
    case UA_DATATYPEKIND_UNION:
    case UA_DATATYPEKIND_LOCALIZEDTEXT:
    case UA_DATATYPEKIND_QUALIFIEDNAME:
        if (timefrom) {
            const UA_DataType *timeMemberType;
            UA_Boolean timeIsArray;
            UA_Boolean timeIsOptional;
            size_t timeOffset;

            if (UA_DataType_getStructMemberExt(type, timefrom->c_str(),
                            &timeOffset,
                            &timeMemberType,
                            &timeIsArray,
                            &timeIsOptional)) {
                if (typeKindOf(timeMemberType) != UA_TYPES_DATETIME || timeIsArray) {
                    errlogPrintf("%s: timestamp element %s has invalid type %s%s - using "
                                 "source timestamp\n",
                                 pitem->recConnector->getRecordName(),
                                 timefrom->c_str(),
                                 typeKindName(typeKindOf(timeMemberType)),
                                 timeIsArray ? "[]" : "");
                } else
                    timesrc = timeOffset;
            } else {
                errlogPrintf(
                    "%s: timestamp element %s not found - using source timestamp\n",
                    pitem->recConnector->getRecordName(),
                    timefrom->c_str());
            }
        }

        for (auto &it : elements) {
            auto pelem = it.lock();
            if ((pelem->index = UA_DataType_getStructMemberExt(type, pelem->name.c_str(),
                            &pelem->offset,
                            &pelem->memberType,
                            &pelem->isArray,
                            &pelem->isOptional))) {
                if (debug() >= 5)
                    std::cout << typeKindName(typeKindOf(type))
                              << " " << pelem
                              << " index=" << pelem->index
                              << " offset=" << pelem->offset
                              << " type=" << variantTypeString(pelem->memberType)
                              << (pelem->isArray ? "[]" : "")
                              << (pelem->isOptional ? " optional" : "")
                              << std::endl;
                if (typeKindOf(pelem->memberType) == UA_DATATYPEKIND_ENUM) {
                    pelem->enumChoices = pitem->session->getEnumChoices(&pelem->memberType->typeId);
                }
            } else {
                std::cerr << "Item " << pitem
                          << ": element " << pelem->name
                          << " not found in " << variantTypeString(type)
                          << std::endl;
            }
        }
        if (debug() >= 5)
            std::cout << " ** " << elements.size()
                      << " child elements mapped to "
                      << variantTypeString(type)
                      << " of " << type->membersSize << " elements" << std::endl;
        break;
    default:
        std::cerr << "Error: " << this
                  << " is no structured data but a " << typeKindName(typeKindOf(type))
                  << std::endl;
    }
    mapped = true;
}

// Getting the timestamp and status information from the Item assumes that only one thread
// is pushing data into the Item's DataElement structure at any time.
void
DataElementOpen62541::setIncomingData (const UA_Variant &value,
                                       ProcessReason reason,
                                       const std::string *timefrom)
{
    // Cache this element. We can make a shallow copy because
    // ItemOpen62541::setIncomingData marks the original response data as "ours".
    // Member data is owned by the [ROOT] element.
    UA_Variant_clear(&incomingData);
    incomingData = value;

    if (isLeaf()) {
        if (pconnector->state() == ConnectionStatus::initialRead && typeKindOf(value) == UA_DATATYPEKIND_ENUM) {
            enumChoices = pitem->session->getEnumChoices(&value.type->typeId);
        }
        if ((pconnector->state() == ConnectionStatus::initialRead
             && (reason == ProcessReason::readComplete || reason == ProcessReason::readFailure))
            || (pconnector->state() == ConnectionStatus::up)) {

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
                          << "/" << incomingQueue.capacity() << ")"
                          << std::endl;
            if (wasFirst)
                pconnector->requestRecordProcessing(reason);
        }
    } else {
        if (UA_Variant_isEmpty(&value))
            return;

        if (debug() >= 5)
            std::cout << "Item " << pitem
                      << " element " << name
                      << " splitting structured data to "
                      << elements.size() << " child elements"
                      << std::endl;

        const UA_DataType *type = value.type;
        char* container = static_cast<char*>(value.data);
        if (typeKindOf(type) == UA_DATATYPEKIND_EXTENSIONOBJECT) {
            UA_ExtensionObject &extensionObject = *reinterpret_cast<UA_ExtensionObject *>(container);
            if (extensionObject.encoding >= UA_EXTENSIONOBJECT_DECODED) {
                // Access content of decoded extension objects
                type = extensionObject.content.decoded.type;
                container = static_cast<char*>(extensionObject.content.decoded.data);
            } else {
                std::cerr << "Cannot get a structure definition for item " << pitem
                          << " because binaryEncodingId " << extensionObject.content.encoded.typeId
                          << " is not in the type dictionary." << std::endl;
                return;
            }
        }

        if (!mapped)
            createMap(type, timefrom);

        if (timesrc >= 0)
            pitem->tsData = ItemOpen62541::uaToEpicsTime(*reinterpret_cast<UA_DateTime*>(container + timesrc), 0);
        else
            pitem->tsData = pitem->tsSource;

        for (auto &it : elements) {
            auto pelem = it.lock();
            const UA_DataType* memberType = pelem->memberType;
            char* memberData = container + pelem->offset;
            UA_Variant memberValue;
            size_t arrayLength = 0; // default to scalar
            if (pelem->isArray) {
                arrayLength = *reinterpret_cast<size_t*>(memberData);
                memberData = *reinterpret_cast<char**>(memberData + sizeof(size_t));
            } else if (pelem->isOptional) {
                /* optional scalar stored through pointer like an array */
                memberData = *reinterpret_cast<char**>(memberData);
            }
            if (type->typeKind == UA_DATATYPEKIND_UNION &&
                    pelem->index != *reinterpret_cast<UA_UInt32*>(container)) {
                // union option not taken
                memberData = nullptr;
            }
            UA_Variant_setArray(&memberValue, memberData, arrayLength, memberType);
            memberValue.storageType = UA_VARIANT_DATA_NODELETE; // Keep ownership of data
            if (debug() && !memberData) {
                std::cerr << pitem->recConnector->getRecordName()
                          << " " << pelem
                          << (type->typeKind == UA_DATATYPEKIND_UNION ? " not taken choice " : " absent optional ")
                          << variantTypeString(memberType)
                          << (pelem->isArray ? " array" : " scalar" )
                          << std::endl;
            }
            pelem->setIncomingData(memberValue, memberData ? reason : ProcessReason::readFailure);
        }
    }
}

void
DataElementOpen62541::setIncomingEvent (ProcessReason reason)
{
    if (isLeaf()) {
        Guard(pconnector->lock);
        if (reason == ProcessReason::connectionLoss && enumChoices)
            enumChoices = nullptr;
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

void
DataElementOpen62541::setState(const ConnectionStatus state)
{
    if (isLeaf()) {
        Guard(pconnector->lock);
        pconnector->setState(state);
    } else {
        for (auto &it : elements) {
            auto pelem = it.lock();
            pelem->setState(state);
        }
    }
}

// Helper to update one data structure element from pointer to child
bool
DataElementOpen62541::updateDataInStruct(void* container,
                                         std::shared_ptr<DataElementOpen62541> pelem)
{
    bool updated = false;
    { // Scope of Guard G
        Guard G(pelem->outgoingLock);
        if (pelem->isDirty()) {
            char* memberData = static_cast<char*>(container) + pelem->offset;
            const UA_Variant& elementData = pelem->getOutgoingData();
            const UA_DataType* memberType = pelem->memberType;
            assert(memberType == elementData.type ||
                (typeKindOf(memberType) == UA_DATATYPEKIND_ENUM &&
                 typeKindOf(elementData.type) == UA_DATATYPEKIND_INT32));
            if (!pelem->isArray && !pelem->isOptional) {
                // mandatory scalar: shallow copy
                UA_clear(memberData, memberType);
                void* data = pelem->moveOutgoingData();
                if (typeKindOf(outgoingData) == UA_DATATYPEKIND_UNION) {
                    *reinterpret_cast<UA_UInt32*>(container) = pelem->index;
                }
                memcpy(memberData, data, memberType->memSize);
                UA_free(data);
            } else {
                // array or optional scalar: move content
                void **memberDataPtr;
                if (pelem->isArray) /* mandatory or optional array stored as length and pointer */ {
                    size_t& arrayLength = *reinterpret_cast<size_t*>(memberData);
                    memberDataPtr = reinterpret_cast<void**>(memberData + sizeof(size_t));
                    UA_Array_delete(*memberDataPtr, arrayLength, memberType);
                    arrayLength = elementData.arrayLength;
                } else /* optional scalar stored through pointer */ {
                    memberDataPtr = reinterpret_cast<void**>(memberData);
                    if (*memberDataPtr) /* absent optional has nullptr here */
                        UA_Array_delete(*memberDataPtr, 1, memberType);
                }
                *memberDataPtr = pelem->moveOutgoingData();
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

        if (typeKindOf(type) == UA_DATATYPEKIND_EXTENSIONOBJECT) {
            UA_ExtensionObject &extensionObject = *reinterpret_cast<UA_ExtensionObject *>(container);
            if (extensionObject.encoding >= UA_EXTENSIONOBJECT_DECODED) {
                // Access content decoded extension objects
                type = extensionObject.content.decoded.type;
                container = extensionObject.content.decoded.data;
            } else {
                std::cerr << "Cannot get a structure definition for item " << pitem
                          << " because binaryEncodingId " << extensionObject.content.encoded.typeId
                          << " is not in the type dictionary." << std::endl;
                return outgoingData;
            }
        }

        if (!mapped)
            createMap(type);

        for (auto &it : elements) {
            auto pelem = it.lock();
            if (updateDataInStruct(container, pelem))
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

// Helper functions to convert between UA_ByteString and hex encoded C string
static size_t printByteString(const UA_ByteString& byteString, char* encoded, size_t len)
{
    static const char hexdigits[] = "0123456789ABCDEF";
    size_t i, l = 0;
    for (i = 0; i < byteString.length && l < len-3; i++) {
        encoded[l++] = hexdigits[byteString.data[i] >> 4];
        encoded[l++] = hexdigits[byteString.data[i] & 0xf];
    }
    encoded[l] = 0;
    return l;
}

static int parseByteString(UA_ByteString& byteString, const char* encoded, int len)
{
    UA_ByteString_clear(&byteString);
    byteString.data = static_cast<UA_Byte*>(UA_malloc(len+1/2)); // can never be longer
    if (!byteString.data) return -1;
    int l = 0;
    bool firstDigit = true;
    while (len--) {
        UA_Byte c, b;
        c = *encoded++;
        if (!c) break;
        if (isblank(c)) {
            firstDigit = true;
            continue;
        }
        if (!isxdigit(c)) {
            // invalid character
            UA_ByteString_clear(&byteString);
            return -1;
        }
        b = c - (isdigit(c) ? '0' : isupper(c) ? 'A'-10 : 'a'-10);
        if (len && isxdigit(c = *encoded)) {
            firstDigit = false;
            encoded++;
            len--;
            b <<= 4;
            b |= c - (isdigit(c) ? '0' : isupper(c) ? 'A'-10 : 'a'-10);
        } else {
            if (!firstDigit || (c && !isblank(c) && !isxdigit(c))) {
                // 1 is the only odd number of digits allowed
                // because otherwise where is the byte border? 12|3 or 1|23 ?
                UA_ByteString_clear(&byteString);
                return -1;
            }
        }
        byteString.data[l++] = b;
    }
    byteString.length = l;
    return l;
}

// CString type needs specialization
long
DataElementOpen62541::readScalar (char *value, const epicsUInt32 len,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *lenRead,
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
    dbgReadScalar(upd.get(), "CString", len);

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
        if (len && value) {
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

                UA_String buffer = UA_STRING_NULL;
                UA_String *datastring = &buffer;
                size_t n = len-1;

                UA_Variant &variant = upd->getData();
                void* payload = variant.data;
                const UA_DataType* type = variant.type;

                if (type->typeKind == UA_DATATYPEKIND_UNION)
                {
                    UA_UInt32 switchfield = *static_cast<UA_UInt32*>(payload)-1;
                    if (switchfield >= type->membersSize) {
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                        break;
                    }
                    const UA_DataTypeMember *member = &type->members[switchfield];
                    payload = static_cast<char*>(payload) + member->padding;
                    type = memberTypeOf(type, member);

                    // prefix value string with switch choice name
                    size_t l = snprintf(value, n, "%s:", member->memberName);
                    value += l;
                    n -= l;
                }

                switch (type->typeKind) {
                case UA_DATATYPEKIND_STRING:
                case UA_DATATYPEKIND_XMLELEMENT:
                {
                    datastring = static_cast<UA_String*>(payload);
                    break;
                }
                case UA_DATATYPEKIND_LOCALIZEDTEXT:
                {
                    datastring = &static_cast<UA_LocalizedText*>(payload)->text;
                    break;
                }
                case UA_DATATYPEKIND_QUALIFIEDNAME:
                {
                    datastring = &static_cast<UA_QualifiedName*>(payload)->name;
                    break;
                }
                case UA_DATATYPEKIND_BYTESTRING:
                {
                    n = printByteString(*static_cast<UA_ByteString*>(payload), value, len);
                    datastring = nullptr;
                    break;
                }
                case UA_DATATYPEKIND_DATETIME:
                {
                    // UA_print does not correct printed time for time zone
                    UA_Int64 tOffset = UA_DateTime_localTimeUtcOffset();
                    UA_DateTime dt = *static_cast<UA_DateTime*>(payload);
                    dt += tOffset;
                    UA_print(&dt, type, &buffer);
                    break;
                }
                case UA_DATATYPEKIND_BYTE:
                case UA_DATATYPEKIND_SBYTE:
                {
                    buffer.data = static_cast<UA_Byte*>(payload);
                    buffer.length = UA_Variant_isScalar(&variant) ? 1 : variant.arrayLength;
                    variant.storageType = UA_VARIANT_DATA_NODELETE; // we have moved ownership
                    n++;
                    break;
                }
                case UA_DATATYPEKIND_ENUM:
                case UA_DATATYPEKIND_INT32:
                {
                    if (enumChoices) {
                        auto it = enumChoices->find(*static_cast<UA_UInt32*>(payload));
                        if (it != enumChoices->end()) {
                            buffer = UA_String_fromChars(it->second.c_str());
                            break;
                        }
                    }
                    // no enum or index not found: fall through
                }
                default:
                    if (type)
                        UA_print(payload, type, &buffer);
                };
                if (datastring) {
                    if (n > datastring->length)
                        n = datastring->length;
                    memcpy(value, reinterpret_cast<char*>(datastring->data), n);
                }
                memset(value+n, 0, len-n);
                if (lenRead)
                    *lenRead = static_cast<epicsUInt32>(n);
                UA_String_clear(&buffer);
                prec->udf = false;
                UA_Variant_clear(&variant);
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
DataElementOpen62541::readArray (char *value, epicsUInt32 len,
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

    // clear *old* array content
    memset(value, 0, *numRead * len);

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
                UA_Variant &variant = upd->getData();
                if (UA_Variant_isScalar(&variant)) {
                    errlogPrintf("%s : incoming data is not an array\n", prec->name);
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    if (UA_STATUS_IS_UNCERTAIN(stat)) {
                        (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                    }
                    elemsWritten = static_cast<epicsUInt32>(num < variant.arrayLength ? num : variant.arrayLength);
                    switch (typeKindOf(variant.type)) {
                    case UA_DATATYPEKIND_STRING:
                    case UA_DATATYPEKIND_XMLELEMENT:
                        for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                            const UA_String& str = static_cast<UA_String *>(variant.data)[i];
                            size_t l = str.length;
                            if (l >= len) l = len-1;
                            memcpy(value + i * len, str.data, l);
                        }
                        break;
                    case UA_DATATYPEKIND_LOCALIZEDTEXT:
                        for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                            const UA_String& str = static_cast<UA_LocalizedText *>(variant.data)[i].text;
                            size_t l = str.length;
                            if (l >= len) l = len-1;
                            memcpy(value + i * len, str.data, l);
                        }
                        break;
                    case UA_DATATYPEKIND_QUALIFIEDNAME:
                        for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                            const UA_String& str = static_cast<UA_QualifiedName *>(variant.data)[i].name;
                            size_t l = str.length;
                            if (l >= len) l = len-1;
                            memcpy(value + i * len, str.data, l);
                        }
                        break;
                    case UA_DATATYPEKIND_BYTESTRING:
                        for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                            printByteString(static_cast<UA_ByteString *>(variant.data)[i], value + i * len, len);
                        }
                        break;
                    default:
                        errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                     prec->name, variantTypeString(variant), epicsTypeString(value));
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                        ret = 1;
                    }
                    prec->udf = false;
                }
                UA_Variant_clear(&variant);
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

// Specialization for epicsUInt8
//   needed because epicsUInt8 is used for arrays of UA_BYTE and UA_BOOLEAN
// CAVEAT: changes in the template (in DataElementOpen62541.h) must be reflected here
template<>
long
DataElementOpen62541::readArray<epicsUInt8> (epicsUInt8 *value, const epicsUInt32 num,
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
    dbgReadArray(upd.get(), num, epicsTypeString(*value));

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
            } else  {
                // Valid OPC UA value, so try to convert
                UA_Variant &variant = upd->getData();
                if (UA_Variant_isScalar(&variant) && variant.type == &UA_TYPES[UA_TYPES_BYTESTRING]) {
                    const UA_ByteString& bs = *static_cast<UA_ByteString*>(variant.data);
                    elemsWritten = static_cast<epicsUInt32>(bs.length);
                    if (elemsWritten > num)
                        elemsWritten = num;
                    memcpy(value, bs.data, elemsWritten);
                } else if (UA_Variant_isScalar(&variant)) {
                    errlogPrintf("%s : incoming data is not an array\n", prec->name);
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else if (typeKindOf(variant) != UA_DATATYPEKIND_BYTE && typeKindOf(variant) != UA_DATATYPEKIND_BOOLEAN) {
                    errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                 prec->name, variantTypeString(variant), epicsTypeString(*value));
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    if (UA_STATUS_IS_UNCERTAIN(stat)) {
                        (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                    }
                    elemsWritten = static_cast<epicsUInt32>(variant.arrayLength);
                    if (elemsWritten > num)
                        elemsWritten = num;
                    memcpy(value, variant.data, elemsWritten);
                    prec->udf = false;
                }
                UA_Variant_clear(&variant);
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
DataElementOpen62541::readArray (char *value, epicsUInt32 len,
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
DataElementOpen62541::writeScalar (const char *value, epicsUInt32 len, dbCommon *prec)
{
    long ret = 1;
    UA_StatusCode status = UA_STATUSCODE_BADUNEXPECTEDERROR;
    long l;
    unsigned long ul;
    double d;
    char* end = nullptr;

    { // Scope of Guard G
        Guard G(outgoingLock);
        UA_Variant_clear(&outgoingData);  // unlikely but we may still have unsent old data to discard
        const UA_DataType* type = incomingData.type;

        int switchfield = -1;
        if (typeKindOf(type) == UA_DATATYPEKIND_UNION) {
            if (value[0] == '\0') {
                switchfield = 0;
            } else for (UA_UInt32 i = 0; i < type->membersSize; i++) {
                epicsUInt32 namelen = static_cast<epicsUInt32>(strlen(type->members[i].memberName));
                if (strncmp(value, type->members[i].memberName, namelen) == 0
                        && value[namelen] == ':') {
                    value += namelen+1;
                    len -= namelen+1;
                    switchfield = i+1;
                    type = memberTypeOf(type, &type->members[i]);
                }
            }
        }

        switch (typeKindOf(type)) {
        case UA_DATATYPEKIND_STRING:
        case UA_DATATYPEKIND_XMLELEMENT:
        {
            UA_String val;
            val.length = strnlen(value, len);
            val.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(value));
            status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
            markAsDirty();
            ret = 0;
            break;
        }
        case UA_DATATYPEKIND_LOCALIZEDTEXT:
        {
            UA_LocalizedText val;
            const char* sep = static_cast<const char*>(memchr(value, '|', len));
            if (sep) {
                val.locale.length = sep - value;
                val.locale.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(value));
                len -= static_cast<epicsUInt32>(sep + 1 - value);
                value = sep + 1;
            } else { // keep the locale
                val.locale = reinterpret_cast<const UA_LocalizedText*>(incomingData.data)->locale;
            }
            val.text.length = strnlen(value, len);
            val.text.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(value));
            status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
            markAsDirty();
            ret = 0;
            break;
        }
        case UA_DATATYPEKIND_QUALIFIEDNAME:
        {
            UA_QualifiedName val;
            const char* sep = static_cast<const char*>(memchr(value, '|', len));
            if (sep) {
                val.namespaceIndex = atoi(value);
                len -= static_cast<epicsUInt32>(sep + 1 - value);
                value = sep + 1;
            } else { // keep the namespace
                val.namespaceIndex = reinterpret_cast<const UA_QualifiedName*>(incomingData.data)->namespaceIndex;
            }
            val.name.length = strnlen(value, len);
            val.name.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(value));
            status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
            markAsDirty();
            ret = 0;
            break;
        }
        case UA_DATATYPEKIND_BYTESTRING:
        {
            UA_ByteString val;
            UA_ByteString_init(&val);
            if (parseByteString(val, value, len) >= 0) {
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
                markAsDirty();
                ret = 0;
            }
            break;
        }
        case UA_DATATYPEKIND_BOOLEAN:
        {
            UA_Boolean val = strchr("YyTt1", *value) != NULL;
            status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
            markAsDirty();
            ret = 0;
            break;
        }
        case UA_DATATYPEKIND_BYTE:
            ul = strtoul(value, &end, 0);
            if (end != value && isWithinRange<UA_Byte>(ul)) {
                UA_Byte val = static_cast<UA_Byte>(ul);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
                markAsDirty();
                ret = 0;
            }
            break;
        case UA_DATATYPEKIND_SBYTE:
            l = strtol(value, &end, 0);
            if (end != value && isWithinRange<UA_SByte>(l)) {
                UA_SByte val = static_cast<UA_Byte>(l);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
                markAsDirty();
                ret = 0;
            }
            break;
        case UA_DATATYPEKIND_UINT16:
            ul = strtoul(value, &end, 0);
            if (end != value && isWithinRange<UA_UInt16>(ul)) {
                UA_UInt16 val = static_cast<UA_UInt16>(ul);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
                markAsDirty();
                ret = 0;
            }
            break;
        case UA_DATATYPEKIND_INT16:
            l = strtol(value, &end, 0);
            if (end != value && isWithinRange<UA_Int16>(l)) {
                UA_Int16 val = static_cast<UA_Int16>(l);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
                markAsDirty();
                ret = 0;
            }
            break;
        case UA_DATATYPEKIND_UINT32:
            ul = strtoul(value, &end, 0);
            if (end != value && isWithinRange<UA_UInt32>(ul)) {
                UA_UInt32 val = static_cast<UA_UInt32>(ul);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
                markAsDirty();
                ret = 0;
            }
            break;
        case UA_DATATYPEKIND_ENUM:
        case UA_DATATYPEKIND_INT32:
            l = strtol(value, &end, 0);
            if (enumChoices) {
                // first test enum strings then numeric values
                // in case a string starts with a number but means a different value
                for (auto it: *enumChoices)
                    if (it.second == value) {
                        l = static_cast<long>(it.first);
                        ret = 0;
                        end = nullptr;
                        break;
                    }
                if (ret != 0 && end != value)
                    for (auto it: *enumChoices)
                        if (l == it.first) {
                            ret = 0;
                            break;
                        }
                if (ret != 0)
                    break;
            }
            if (end != value && isWithinRange<UA_Int32>(l)) {
                UA_Int32 val = static_cast<UA_Int32>(l);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT32]);
                markAsDirty();
                ret = 0;
            }
            break;
        case UA_DATATYPEKIND_UINT64:
            ul = strtoul(value, &end, 0);
            if (end != value && isWithinRange<UA_UInt64>(ul)) {
                UA_UInt64 val = static_cast<UA_UInt64>(ul);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
                markAsDirty();
                ret = 0;
            }
            break;
        case UA_DATATYPEKIND_INT64:
            l = strtol(value, &end, 0);
            if (end != value && isWithinRange<UA_Int64>(l)) {
                UA_Int64 val = static_cast<UA_Int64>(l);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
                markAsDirty();
                ret = 0;
            }
            break;
        case UA_DATATYPEKIND_FLOAT:
            d = strtod(value, &end);
            if (end != value && isWithinRange<UA_Float>(d)) {
                UA_Float val = static_cast<UA_Float>(d);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
                markAsDirty();
                ret = 0;
            }
            break;
        case UA_DATATYPEKIND_DOUBLE:
        {
            d = strtod(value, &end);
            if (end != value) {
                UA_Double val = static_cast<UA_Double>(d);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, type);
            }
            break;
        }
        default:
            errlogPrintf("%s : unsupported conversion from string to %s for outgoing data\n",
                         prec->name, variantTypeString(incomingData));
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        }
        if (switchfield >= 0) {
            // manually wrap value from outgoingData into union
            void *p = UA_new(type);
            if (p) {
                *static_cast<UA_UInt32*>(p) = switchfield;
                if (switchfield > 0) {
                    memcpy(static_cast<char*>(p) + type->members[switchfield-1].padding,
                        outgoingData.data, outgoingData.type->memSize);
                    UA_free(outgoingData.data);
                }
                UA_Variant_setScalar(&outgoingData, p, type);
                status = UA_STATUSCODE_GOOD;
                markAsDirty();
                ret = 0;
            }
        }
    } // Scope of Guard G
    if (ret != 0) {
        errlogPrintf("%s : value \"%s\" out of range\n",
                     prec->name, value);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
    }
    if (ret == 0 && UA_STATUS_IS_BAD(status)) {
        errlogPrintf("%s : scalar copy failed: %s\n",
                     prec->name, UA_StatusCode_name(status));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    }
    if (ret == 0)
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

static inline
UA_String UA_StringNCopy(const char *src, size_t maxlength)
{
    UA_String s;
    s.length = src ? strnlen(src, maxlength) : 0;
    s.data = s.length ? static_cast<UA_Byte*>(UA_malloc(s.length)) : nullptr;
    if (s.data) memcpy(s.data, src, s.length);
    return s;
}

// Write array for EPICS String / UA_String
long
DataElementOpen62541::writeArray (const char *value, epicsUInt32 len,
                              const epicsUInt32 num,
                              const UA_DataType *targetType,
                              dbCommon *prec)
{
    long ret = 0;

    if (UA_Variant_isScalar(&incomingData)) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
        const UA_DataType* type = incomingData.type;
        void* data = UA_Array_new(num, type);
        if (!data) {
            errlogPrintf("%s : out of memory\n", prec->name);
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        } else {
            switch(typeKindOf(type)) {
            case UA_DATATYPEKIND_STRING:
            case UA_DATATYPEKIND_XMLELEMENT:
            case UA_DATATYPEKIND_BYTESTRING: {
                UA_String *arr = static_cast<UA_String *>(data);
                for (epicsUInt32 i = 0; i < num; i++) {
                    arr[i] = UA_StringNCopy(value, len);
                    value += len;
                }
                break;
            }
            case UA_DATATYPEKIND_LOCALIZEDTEXT: {
                UA_LocalizedText *arr = static_cast<UA_LocalizedText *>(data);
                for (epicsUInt32 i = 0; i < num; i++) {
                    const char* sep = static_cast<const char*>(memchr(value, '|', len));
                    if (sep) {
                        arr[i].locale = UA_StringNCopy(value, sep - value);
                    } else if (i < incomingData.arrayLength) {
                        UA_copy(&reinterpret_cast<const UA_LocalizedText*>(
                            incomingData.data)[i].locale,
                            &arr[i].locale, &UA_TYPES[UA_TYPES_STRING]);
                    } else if (i > 0) {
                        UA_copy(&arr[i-1].locale,
                            &arr[i].locale, &UA_TYPES[UA_TYPES_STRING]);
                    }
                    arr[i].text = UA_StringNCopy(sep ? sep+1 : value, sep ? len - (sep+1-value) : len);
                    value += len;
                }
                break;
            }
            case UA_DATATYPEKIND_QUALIFIEDNAME: {
                UA_QualifiedName *arr = static_cast<UA_QualifiedName *>(data);
                for (epicsUInt32 i = 0; i < num; i++) {
                    const char* sep = static_cast<const char*>(memchr(value, '|', len));
                    if (sep) {
                        arr[i].namespaceIndex = atoi(value);
                    } else if (i < incomingData.arrayLength) {
                        arr[i].namespaceIndex = reinterpret_cast<const UA_QualifiedName*>(
                            incomingData.data)[i].namespaceIndex;
                    } else if (i > 0) {
                        arr[i].namespaceIndex = arr[i-1].namespaceIndex;
                    }
                    arr[i].name = UA_StringNCopy(sep ? sep+1 : value, sep ? len - (sep+1-value) : len);
                    value += len;
                }
                break;
            }
            default:
                errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                             prec->name,
                             variantTypeString(incomingData),
                             variantTypeString(targetType),
                             epicsTypeString(value));
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                UA_Array_delete(data, num, type);
                ret = 1;
            }
            { // Scope of Guard G
                Guard G(outgoingLock);
                UA_Variant_clear(&outgoingData);  // unlikely but we may still have unsent old data to discard
                UA_Variant_setArray(&outgoingData, data, num, type); // no copy but move content
                markAsDirty();
            }

            dbgWriteArray(num, epicsTypeString(value));
        }
    }
    return ret;
}

// Specialization for epicsUInt8
//   needed because epicsUInt8 is used for arrays of UA_BYTE and UA_BOOLEAN
// CAVEAT: changes in the template (in DataElementOpen62541.h) must be reflected here
template<>
long
DataElementOpen62541::writeArray<epicsUInt8>(const epicsUInt8 *value,
                                             const epicsUInt32 num,
                                             const UA_DataType *targetType,
                                             dbCommon *prec)
{
    long ret = 0;

    if (UA_Variant_isScalar(&incomingData) && incomingData.type == &UA_TYPES[UA_TYPES_BYTESTRING]) {
        UA_ByteString bs;
        bs.length = num;
        bs.data = static_cast<UA_Byte*>(const_cast<epicsUInt8 *>(value));
        { // Scope of Guard G
            Guard G(outgoingLock);
            UA_Variant_setScalarCopy(&outgoingData, &bs, incomingData.type);
            markAsDirty();
        }

        dbgWriteScalar();
    } else if (UA_Variant_isScalar(&incomingData)) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else if (typeKindOf(incomingData) != UA_DATATYPEKIND_BYTE && typeKindOf(incomingData) != UA_DATATYPEKIND_BOOLEAN) {
        errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                     prec->name,
                     variantTypeString(incomingData),
                     variantTypeString(targetType),
                     epicsTypeString(*value));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
        UA_StatusCode status;
        { // Scope of Guard G
            Guard G(outgoingLock);
            status = UA_Variant_setArrayCopy(&outgoingData, value, num, incomingData.type);
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
    return writeArray<epicsUInt8>(value, num, &UA_TYPES[UA_TYPES_BYTE], prec);
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
DataElementOpen62541::writeArray (const char *value, epicsUInt32 len, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray(value, len, num, &UA_TYPES[UA_TYPES_STRING], prec);
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
