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
#include "DataElementOpen62541Node.h"
#include "ItemOpen62541.h"
#include "RecordConnector.h"

#include <epicsTypes.h>
#include <errlog.h>

#include <open62541/client.h>

#include <iostream>
#include <memory>
#include <string>

namespace DevOpcua {

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

DataElementOpen62541Node::DataElementOpen62541Node (const std::string &name, ItemOpen62541 *item)
    : DataElementOpen62541(name, item)
    , timesrc(-1)
    , mapped(false)
{
    UA_Variant_init(&incomingData);
    UA_Variant_init(&outgoingData);
}

DataElementOpen62541Node::~DataElementOpen62541Node() = default;

void
DataElementOpen62541Node::createMap (const UA_DataType *type, const std::string *timefrom)
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

            if (UA_DataType_getStructMemberExt(
                    type, timefrom->c_str(), &timeOffset, &timeMemberType, &timeIsArray, &timeIsOptional)) {
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
                errlogPrintf("%s: timestamp element %s not found - using source timestamp\n",
                             pitem->recConnector->getRecordName(),
                             timefrom->c_str());
            }
        }

        for (const auto &it : elements) {
            auto pelem = it.lock();
            if ((pelem->index = UA_DataType_getStructMemberExt(type,
                                                               pelem->name.c_str(),
                                                               &pelem->offset,
                                                               &pelem->memberType,
                                                               &pelem->isArray,
                                                               &pelem->isOptional))) {
                if (debug() >= 5)
                    std::cout << typeKindName(typeKindOf(type)) << " " << pelem << " index=" << pelem->index
                              << " offset=" << pelem->offset << " type=" << variantTypeString(pelem->memberType)
                              << (pelem->isArray ? "[]" : "") << (pelem->isOptional ? " optional" : "") << std::endl;
            } else {
                std::cerr << "Item " << pitem << ": element " << pelem->name << " not found in "
                          << variantTypeString(type) << std::endl;
            }
        }
        if (debug() >= 5)
            std::cout << " ** " << elements.size() << " child elements mapped to " << variantTypeString(type) << " of "
                      << type->membersSize << " elements" << std::endl;
        break;
    default:
        std::cerr << "Error: " << this << " is no structured data but a " << typeKindName(typeKindOf(type))
                  << std::endl;
    }
    mapped = true;
}

void
DataElementOpen62541Node::show (const int level, const unsigned int indent) const
{
    std::string ind(indent * 2, ' ');
    std::cout << ind;
    std::cout << "node=" << name << " children=" << elements.size() << " mapped=" << (mapped ? "y" : "n") << "\n";
    for (const auto &it : elements) {
        if (auto pelem = it.lock()) {
            pelem->show(level, indent + 1);
        }
    }
}

// Getting the timestamp and status information from the Item assumes that only one thread
// is pushing data into the Item's DataElement structure at any time.
void
DataElementOpen62541Node::setIncomingData (const UA_Variant &value, ProcessReason reason, const std::string *timefrom)
{
    // Cache this element. We can make a shallow copy because
    // ItemOpen62541::setIncomingData marks the original response data as "ours".
    // Member data is owned by the [ROOT] element.
    UA_Variant_clear(&incomingData);
    incomingData = value;

    if (UA_Variant_isEmpty(&value))
        return;

    if (debug() >= 5)
        std::cout << "Item " << pitem << " element " << name << " splitting structured data to " << elements.size()
                  << " child elements" << std::endl;

    const UA_DataType *type = value.type;
    char *container = static_cast<char *>(value.data);
    if (typeKindOf(type) == UA_DATATYPEKIND_EXTENSIONOBJECT) {
        UA_ExtensionObject &extensionObject = *reinterpret_cast<UA_ExtensionObject *>(container);
        if (extensionObject.encoding >= UA_EXTENSIONOBJECT_DECODED) {
            // Access content of decoded extension objects
            type = extensionObject.content.decoded.type;
            container = static_cast<char *>(extensionObject.content.decoded.data);
        } else {
            std::cerr << "Cannot get a structure definition for item " << pitem << " because binaryEncodingId "
                      << extensionObject.content.encoded.typeId << " is not in the type dictionary." << std::endl;
            return;
        }
    }

    if (!mapped)
        createMap(type, timefrom);

    if (timefrom) {
        if (timesrc >= 0)
            pitem->tsData = ItemOpen62541::uaToEpicsTime(*reinterpret_cast<UA_DateTime *>(container + timesrc), 0);
        else
            pitem->tsData = pitem->tsSource;
    }

    for (const auto &it : elements) {
        auto pelem = it.lock();
        const UA_DataType *memberType = pelem->memberType;
        char *memberData = container + pelem->offset;
        UA_Variant memberValue;
        size_t arrayLength = 0; // default to scalar
        if (pelem->isArray) {
            arrayLength = *reinterpret_cast<size_t *>(memberData);
            memberData = *reinterpret_cast<char **>(memberData + sizeof(size_t));
        } else if (pelem->isOptional) {
            /* optional scalar stored through pointer like an array */
            memberData = *reinterpret_cast<char **>(memberData);
        }
        if (type->typeKind == UA_DATATYPEKIND_UNION && pelem->index != *reinterpret_cast<UA_UInt32 *>(container)) {
            // union option not taken
            memberData = nullptr;
        }
        UA_Variant_setArray(&memberValue, memberData, arrayLength, memberType);
        memberValue.storageType = UA_VARIANT_DATA_NODELETE; // Keep ownership of data
        if (debug() && !memberData) {
            std::cerr << pitem->recConnector->getRecordName() << " " << pelem
                      << (type->typeKind == UA_DATATYPEKIND_UNION ? " not taken choice " : " absent optional ")
                      << variantTypeString(memberType) << (pelem->isArray ? " array" : " scalar") << std::endl;
        }
        pelem->setIncomingData(memberValue, memberData ? reason : ProcessReason::readFailure);
    }
}

void
DataElementOpen62541Node::setIncomingEvent (ProcessReason reason)
{
    for (const auto &it : elements) {
        auto pelem = it.lock();
        pelem->setIncomingEvent(reason);
    }
    if (reason == ProcessReason::connectionLoss) {
        elementMap.clear();
        timesrc = -1;
        mapped = false;
    }
}

void
DataElementOpen62541Node::setState (const ConnectionStatus state)
{
    for (const auto &it : elements) {
        auto pelem = it.lock();
        pelem->setState(state);
    }
}

// Helper to update one data structure element from pointer to child
bool
DataElementOpen62541Node::updateDataInStruct (void *container, std::shared_ptr<DataElementOpen62541> pelem)
{
    bool updated = false;
    { // Scope of Guard G
        Guard G(pelem->outgoingLock);
        if (pelem->isDirty()) {
            char *memberData = static_cast<char *>(container) + pelem->offset;
            const UA_Variant &elementData = pelem->getOutgoingData();
            const UA_DataType *memberType = pelem->memberType;
            assert(memberType == elementData.type
                   || (typeKindOf(memberType) == UA_DATATYPEKIND_ENUM
                       && typeKindOf(elementData.type) == UA_DATATYPEKIND_INT32));
            if (!pelem->isArray && !pelem->isOptional) {
                // mandatory scalar: shallow copy
                UA_clear(memberData, memberType);
                void *data = pelem->moveOutgoingData();
                if (typeKindOf(outgoingData) == UA_DATATYPEKIND_UNION) {
                    *reinterpret_cast<UA_UInt32 *>(container) = pelem->index;
                }
                memcpy(memberData, data, memberType->memSize);
                UA_free(data);
            } else {
                // array or optional scalar: move content
                void **memberDataPtr;
                if (pelem->isArray) /* mandatory or optional array stored as length and pointer */ {
                    size_t &arrayLength = *reinterpret_cast<size_t *>(memberData);
                    memberDataPtr = reinterpret_cast<void **>(memberData + sizeof(size_t));
                    UA_Array_delete(*memberDataPtr, arrayLength, memberType);
                    arrayLength = elementData.arrayLength;
                } else /* optional scalar stored through pointer */ {
                    memberDataPtr = reinterpret_cast<void **>(memberData);
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
            std::cout << "Data from child element " << pelem->name << " inserted into data structure" << std::endl;
        } else {
            std::cout << "Data from child element " << pelem->name << " ignored (not dirty)" << std::endl;
        }
    }
    return updated;
}

const UA_Variant &
DataElementOpen62541Node::getOutgoingData ()
{
    if (debug() >= 4)
        std::cout << "Item " << pitem << " element " << name << " updating structured data from " << elements.size()
                  << " child elements" << std::endl;

    UA_Variant_clear(&outgoingData);
    UA_Variant_copy(&incomingData, &outgoingData);
    isdirty = false;
    const UA_DataType *type = outgoingData.type;
    void *container = outgoingData.data;

    if (typeKindOf(type) == UA_DATATYPEKIND_EXTENSIONOBJECT) {
        UA_ExtensionObject &extensionObject = *reinterpret_cast<UA_ExtensionObject *>(container);
        if (extensionObject.encoding >= UA_EXTENSIONOBJECT_DECODED) {
            // Access content decoded extension objects
            type = extensionObject.content.decoded.type;
            container = extensionObject.content.decoded.data;
        } else {
            std::cerr << "Cannot get a structure definition for item " << pitem << " because binaryEncodingId "
                      << extensionObject.content.encoded.typeId << " is not in the type dictionary." << std::endl;
            return outgoingData;
        }
    }

    if (!mapped)
        createMap(type);

    for (const auto &it : elements) {
        auto pelem = it.lock();
        if (updateDataInStruct(container, pelem))
            isdirty = true;
    }
    if (isdirty) {
        if (debug() >= 4)
            std::cout << "Encoding changed data structure to outgoingData of element " << name << std::endl;
    } else {
        if (debug() >= 4)
            std::cout << "Returning unchanged outgoingData of element " << name << std::endl;
    }
    return outgoingData;
}

void
DataElementOpen62541Node::clearOutgoingData ()
{
    UA_Variant_clear(&outgoingData);
}

void
DataElementOpen62541Node::requestRecordProcessing (const ProcessReason reason) const
{
    for (auto &it : elementMap) {
        auto pelem = it.second.lock();
        pelem->requestRecordProcessing(reason);
    }
}

} // namespace DevOpcua
