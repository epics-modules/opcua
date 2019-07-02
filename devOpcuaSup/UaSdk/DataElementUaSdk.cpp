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

// Avoid problems on Windows (macros min, max clash with numeric_limits<>)
#ifdef _WIN32
#  define NOMINMAX
#endif

#include <iostream>
#include <memory>
#include <limits>
#include <string>
#include <cstring>
#include <cstdlib>

#include <uadatetime.h>
#include <uaextensionobject.h>
#include <uaarraytemplates.h>
#include <opcua_builtintypes.h>

#include <errlog.h>

#include "ItemUaSdk.h"
#include "DataElementUaSdk.h"
#include "UpdateQueue.h"
#include "RecordConnector.h"

namespace DevOpcua {

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

DataElementUaSdk::DataElementUaSdk (const std::string &name,
                                    ItemUaSdk *item,
                                    RecordConnector *pconnector)
    : DataElement(pconnector, name)
    , pitem(item)
    , mapped(false)
    , incomingData(pconnector->plinkinfo->queueSize * 2 + 3, pconnector->plinkinfo->discardOldest)
    , incomingType(OpcUaType_Null)
    , incomingIsArray(false)
{}

DataElementUaSdk::DataElementUaSdk (const std::string &name,
                                    ItemUaSdk *item,
                                    std::weak_ptr<DataElementUaSdk> child)
    : DataElement(name)
    , pitem(item)
    , mapped(false)
    , incomingData(0ul)
    , incomingType(OpcUaType_Null)
    , incomingIsArray(false)
{
    elements.push_back(child);
}

void
DataElementUaSdk::show (const int level, const unsigned int indent) const
{
    std::string ind(indent*2, ' ');
    std::cout << ind;
    if (isLeaf()) {
        std::cout << "leaf=" << name << " record(" << pconnector->getRecordType() << ")="
                  << pconnector->getRecordName()
                  << " type=" << variantTypeString(incomingType) << "\n";
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

void
DataElementUaSdk::addElementChain (ItemUaSdk *item,
                                   RecordConnector *pconnector,
                                   const std::string &fullpath)
{
    bool hasRootElement = true;
    // Create final path element as leaf and link it to connector
    std::string path(fullpath);
    std::string restpath;
    size_t sep = path.find_last_of(separator);
    // allow escaping separators
    while (path[sep-1] == '\\') {
        path.erase(--sep, 1);
        sep = path.find_last_of(separator, --sep);
    }
    std::string leafname = path.substr(sep + 1);
    if (leafname.empty()) leafname = "[ROOT]";
    if (sep != std::string::npos)
        restpath = path.substr(0, sep);

    auto chainelem = std::make_shared<DataElementUaSdk>(leafname, item, pconnector);
    pconnector->setDataElement(chainelem);

    // Starting from item...
    std::weak_ptr<DataElementUaSdk> topelem = item->rootElement;

    if (topelem.expired()) hasRootElement = false;

    // Simple case (leaf is the root element)
    if (leafname == "[ROOT]") {
        if (hasRootElement) throw std::runtime_error(SB() << "root data element already set");
        item->rootElement = chainelem;
        return;
    }

    std::string name;
    if (hasRootElement) {
        // Find the existing part of the path
        bool found;
        do {
            found = false;
            sep = restpath.find_first_of(separator);
            // allow escaping separators
            while (restpath[sep-1] == '\\') {
                    restpath.erase(sep-1, 1);
                    sep = restpath.find_first_of(separator, sep);
            }
            if (sep == std::string::npos)
                name = restpath;
            else
                name = restpath.substr(0, sep);

            // Search for name in list of children
            if (!name.empty()) {
                if (auto pelem = topelem.lock()) {
                    for (auto it : pelem->elements) {
                        if (auto pit = it.lock()) {
                            if (pit->name == name) {
                                found = true;
                                topelem = it;
                                break;
                            }
                        }
                    }
                }
            }
            if (found) {
                if (sep == std::string::npos)
                    restpath.clear();
                else
                    restpath = restpath.substr(sep + 1);
            }
        } while (found && !restpath.empty());
    }
    // At this point, topelem is the element to add the chain to
    // (otherwise, a root element has to be added), and
    // restpath is the remaining chain that has to be created

    // Create remaining chain, bottom up
    while (restpath.length()) {
        sep = restpath.find_last_of(separator);
        // allow escaping separators
        while (restpath[sep-1] == '\\') {
            restpath.erase(--sep, 1);
            sep = restpath.find_last_of(separator, --sep);
        }
        name = restpath.substr(sep + 1);
        if (sep != std::string::npos)
            restpath = restpath.substr(0, sep);
        else
            restpath.clear();

        chainelem->parent = std::make_shared<DataElementUaSdk>(name, item, chainelem);
        chainelem = chainelem->parent;
    }

    // Add to topelem, or create rootelem and add it to item
    if (hasRootElement) {
        if (auto pelem = topelem.lock()) {
            pelem->elements.push_back(chainelem);
            chainelem->parent = pelem;
        } else {
            throw std::runtime_error(SB() << "previously found top element invalidated");
        }
    } else {
        chainelem->parent = std::make_shared<DataElementUaSdk>("[ROOT]", item, chainelem);
        chainelem = chainelem->parent;
        item->rootElement = chainelem;
    }
}

void
DataElementUaSdk::setIncomingData (const UaVariant &value, ProcessReason reason)
{
    incomingType = value.type();
    incomingIsArray = value.isArray();

    if (isLeaf()) {
        Guard(pconnector->lock);
        bool wasFirst = false;
        // Make a copy of the value for this element and put it on the queue
        Update<UaVariant> *u(new Update<UaVariant>(getIncomingTimeStamp(), reason, value));
        incomingData.pushUpdate(std::shared_ptr<Update<UaVariant>>(u), &wasFirst);
        if (debug() >= 5)
            std::cout << "Element " << name << " set data ("
                      << processReasonString(reason)
                      << ") for record " << pconnector->getRecordName()
                      << " (queue use " << incomingData.size()
                      << "/" << incomingData.capacity() << ")" << std::endl;
        if (wasFirst)
            pconnector->requestRecordProcessing(reason);
    } else {
        if (debug() >= 5)
            std::cout << "Element " << name << " splitting data structure to "
                      << elements.size() << " child elements" << std::endl;

        if (value.type() == OpcUaType_ExtensionObject) {
            UaExtensionObject extensionObject;
            value.toExtensionObject(extensionObject);

            // Try to get the structure definition from the dictionary
            UaStructureDefinition definition = pitem->structureDefinition(extensionObject.encodingTypeId());
            if (!definition.isNull()) {
                if (!definition.isUnion()) {
                    // ExtensionObject is a structure
                    // Decode the ExtensionObject to a UaGenericValue to provide access to the structure fields
                    UaGenericStructureValue genericValue;
                    genericValue.setGenericValue(extensionObject, definition);

                    if (!mapped) {
                        if (debug() >= 5)
                            std::cout << " ** creating index-to-element map for child elements" << std::endl;
                        for (auto &it : elements) {
                            auto pelem = it.lock();
                            for (int i = 0; i < definition.childrenCount(); i++) {
                                if (pelem->name == definition.child(i).name().toUtf8()) {
                                    elementMap.insert({i, it});
                                    pelem->setIncomingData(genericValue.value(i), reason);
                                }
                            }
                        }
                        if (debug() >= 5)
                            std::cout << " ** " << elementMap.size() << "/" << elements.size()
                                      << " child elements mapped to a "
                                      << "structure of " << definition.childrenCount() << " elements" << std::endl;
                        mapped = true;
                    } else {
                        for (auto &it : elementMap) {
                            auto pelem = it.second.lock();
                            pelem->setIncomingData(genericValue.value(it.first), reason);
                        }
                    }
                }

            } else
                errlogPrintf("Cannot get a structure definition for %s - check access to type dictionary\n",
                             extensionObject.dataTypeId().toString().toUtf8());
        }
    }
}

void
DataElementUaSdk::setIncomingEvent (ProcessReason reason)
{
    if (isLeaf()) {
        Guard(pconnector->lock);
        bool wasFirst = false;
        // Put the event on the queue
        Update<UaVariant> *u(new Update<UaVariant>(getIncomingTimeStamp(), reason));
        incomingData.pushUpdate(std::shared_ptr<Update<UaVariant>>(u), &wasFirst);
        if (debug() >= 5)
            std::cout << "Element " << name << " set event ("
                      << processReasonString(reason)
                      << ") for record " << pconnector->getRecordName()
                      << " (queue use " << incomingData.size()
                      << "/" << incomingData.capacity() << ")" << std::endl;
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
DataElementUaSdk::checkIncomingEmpty () const
{
    if (incomingData.empty())
        throw std::runtime_error(SB() << "no incoming data");
}

void
DataElementUaSdk::logReadScalar (const Update<UaVariant> *upd,
                                 const std::string &targetTypeName) const
{
    if (isLeaf() && debug()) {
        char time_buf[40];
        UaVariant &data = upd->getData();
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        std::cout << pconnector->getRecordName() << ": ("
                  << ( pconnector->plinkinfo->useServerTimestamp ? "server" : "device") << " time "
                  << time_buf << ") reading ";
        if (data.type() == OpcUaType_String)
            std::cout << "'" << data.toString().toUtf8() << "'";
        else
            std::cout << data.toString().toUtf8();
        std::cout << " (" << variantTypeString(data.type()) << ")"
                  << " as " << targetTypeName
                  << " (remaining queue " << incomingData.size()
                  << "/" << incomingData.capacity() << ")" << std::endl;

    }
}

void
DataElementUaSdk::checkReadArray (const Update<UaVariant> *upd,
                                  OpcUa_BuiltInType expectedType,
                                  const epicsUInt32 targetSize,
                                  const std::string &targetTypeName) const
{
    UaVariant &data = upd->getData();
    if (!incomingIsArray)
        throw std::runtime_error(SB() << "incoming data is not an array");
    if (incomingType != expectedType)
        throw std::runtime_error(SB() << "incoming array data type ("
                                 << variantTypeString(data.type()) << ")"
                                 << " does not match EPICS array type (" << targetTypeName << ")");
    if (isLeaf() && debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        std::cout << pconnector->getRecordName() << ": ("
                  << ( pconnector->plinkinfo->useServerTimestamp ? "server" : "device") << " time "
                  << time_buf << ") reading "
                  << "array of " << variantTypeString(data.type())
                  << "[" << data.arraySize() << "]"
                  << " into " << targetTypeName << "[" << targetSize << "]"
                  << " (remaining queue " << incomingData.size()
                  << "/" << incomingData.capacity() << ")" << std::endl;
    }
}

epicsInt32
DataElementUaSdk::readInt32 (ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    logReadScalar(upd.get(), "Int32");

    OpcUa_Int32 v;
    if (OpcUa_IsNotGood(upd->getData().toInt32(v)))
        throw std::runtime_error(SB() << "incoming data out-of-bounds");
    if (ts) *ts = upd->getTimeStamp();
    return v;
}

epicsInt64
DataElementUaSdk::readInt64 (ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    logReadScalar(upd.get(), "Int64");

    OpcUa_Int64 v;
    if (OpcUa_IsNotGood(upd->getData().toInt64(v)))
        throw std::runtime_error(SB() << "incoming data out-of-bounds");
    if (ts) *ts = upd->getTimeStamp();
    return v;
}

epicsUInt32
DataElementUaSdk::readUInt32 (ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    logReadScalar(upd.get(), "UInt32");

    OpcUa_UInt32 v;
    if (OpcUa_IsNotGood(upd->getData().toUInt32(v)))
        throw std::runtime_error(SB() << "incoming data out-of-bounds");
    if (ts) *ts = upd->getTimeStamp();
    return v;
}

epicsFloat64
DataElementUaSdk::readFloat64 (ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    logReadScalar(upd.get(), "Float64");

    OpcUa_Double v;
    if (OpcUa_IsNotGood(upd->getData().toDouble(v)))
        throw std::runtime_error(SB() << "incoming data out-of-bounds");
    if (ts) *ts = upd->getTimeStamp();
    return v;
}

void
DataElementUaSdk::readCString (char *value, const size_t num,
                               ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    if (isLeaf() && debug()) {
        std::cout << pconnector->getRecordName() << ": reading ";
        if (upd->getData().type() == OpcUaType_String)
            std::cout << "'" << upd->getData().toString().toUtf8() << "'";
        else
            std::cout << upd->getData().toString().toUtf8();
        std::cout << " (" << variantTypeString(upd->getData().type()) << ")"
                  << " as CString [" << num << "]" << std::endl;
    }

    if (num > 0) {
        strncpy(value, upd->getData().toString().toUtf8(), num);
        value[num-1] = '\0';
    }

    if (ts) *ts = upd->getTimeStamp();
}

epicsUInt32
DataElementUaSdk::readArrayInt8 (epicsInt8 *value, epicsUInt32 num,
                                 ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_SByte, num, "epicsInt8");

    UaSByteArray arr;
    upd->getData().toSByteArray(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    memcpy(value, arr.rawData(), sizeof(epicsInt8) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayUInt8 (epicsUInt8 *value, epicsUInt32 num,
                                  ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_Byte, num, "epicsUInt8");

    UaByteArray arr;
    upd->getData().toByteArray(arr);
    epicsUInt32 no_elems = static_cast<epicsUInt32>(arr.size());
    if (num < no_elems) no_elems = num;
    memcpy(value, arr.data(), sizeof(epicsUInt8) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayInt16 (epicsInt16 *value, epicsUInt32 num,
                                  ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_Int16, num, "epicsInt16");

    UaInt16Array arr;
    upd->getData().toInt16Array(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    memcpy(value, arr.rawData(), sizeof(epicsInt16) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayUInt16 (epicsUInt16 *value, epicsUInt32 num,
                                   ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_UInt16, num, "epicsUInt16");

    UaUInt16Array arr;
    upd->getData().toUInt16Array(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    memcpy(value, arr.rawData(), sizeof(epicsUInt16) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayInt32 (epicsInt32 *value, epicsUInt32 num,
                                  ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_Int32, num, "epicsInt32");

    UaInt32Array arr;
    upd->getData().toInt32Array(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    memcpy(value, arr.rawData(), sizeof(epicsInt32) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayUInt32 (epicsUInt32 *value, epicsUInt32 num,
                                   ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_UInt32, num, "epicsUInt32");

    UaUInt32Array arr;
    upd->getData().toUInt32Array(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    memcpy(value, arr.rawData(), sizeof(epicsUInt32) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayInt64 (epicsInt64 *value, epicsUInt32 num,
                                  ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_Int64, num, "epicsInt64");

    UaInt64Array arr;
    upd->getData().toInt64Array(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    memcpy(value, arr.rawData(), sizeof(epicsInt64) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayUInt64 (epicsUInt64 *value, epicsUInt32 num,
                                   ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_UInt64, num, "epicsUInt64");

    UaUInt64Array arr;
    upd->getData().toUInt64Array(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    memcpy(value, arr.rawData(), sizeof(epicsUInt64) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayFloat32 (epicsFloat32 *value, epicsUInt32 num,
                                    ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_Float, num, "epicsFloat32");

    UaFloatArray arr;
    upd->getData().toFloatArray(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    memcpy(value, arr.rawData(), sizeof(epicsFloat32) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayFloat64 (epicsFloat64 *value, epicsUInt32 num,
                                    ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_Double, num, "epicsFloat64");

    UaDoubleArray arr;
    upd->getData().toDoubleArray(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    memcpy(value, arr.rawData(), sizeof(epicsFloat64) * no_elems);

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

epicsUInt32
DataElementUaSdk::readArrayOldString (epicsOldString *value, epicsUInt32 num,
                                      ProcessReason *nextReason, epicsTimeStamp *ts)
{
    checkIncomingEmpty();
    std::shared_ptr<Update<UaVariant>> upd = incomingData.popUpdate(nextReason);

    checkReadArray(upd.get(), OpcUaType_String, num, "epicsOldString");

    UaStringArray arr;
    upd->getData().toStringArray(arr);
    epicsUInt32 no_elems = num < arr.length() ? num : arr.length();
    for (epicsUInt32 i = 0; i < num; i++) {
        strncpy(value[i], UaString(arr[i]).toUtf8(), MAX_STRING_SIZE);
    }

    if (ts) *ts = upd->getTimeStamp();

    return no_elems;
}

bool
DataElementUaSdk::readWasOk () const
{
    bool status = !!pitem->getReadStatus().isGood();

    if (isLeaf() && debug())
        std::cout << pconnector->getRecordName()
                  << ": read status is '"
                  << pitem->getReadStatus().toString().toUtf8() << "'"
                  << std::endl;
    return status;
}

bool
DataElementUaSdk::writeWasOk () const
{
    bool status = !!pitem->getWriteStatus().isGood();
    if (isLeaf() && debug())
        std::cout << pconnector->getRecordName()
                  << ": write status is '"
                  << pitem->getWriteStatus().toString().toUtf8() << "'"
                  << std::endl;
    return status;
}

template<typename FROM, typename TO>
void checkRange (const FROM &value) {
    if (value < std::numeric_limits<TO>::min() || value > std::numeric_limits<TO>::max())
        throw std::runtime_error(SB() << "outgoing data out-of-bounds");
}

// Specialization for signed/unsigned to avoid compiler warnings
template<>
void checkRange<epicsInt32, OpcUa_UInt32> (const epicsInt32 &value) {
    if (value < 0)
        throw std::runtime_error(SB() << "outgoing data out-of-bounds");
}

template<>
void checkRange<epicsUInt32, OpcUa_SByte> (const epicsUInt32 &value) {
    if (value > static_cast<OpcUa_UInt32>(std::numeric_limits<OpcUa_SByte>::max()))
        throw std::runtime_error(SB() << "outgoing data out-of-bounds");
}

template<>
void checkRange<epicsUInt32, OpcUa_Int16> (const epicsUInt32 &value) {
    if (value > static_cast<OpcUa_UInt32>(std::numeric_limits<OpcUa_Int16>::max()))
        throw std::runtime_error(SB() << "outgoing data out-of-bounds");
}

template<>
void checkRange<epicsUInt32, OpcUa_Int32> (const epicsUInt32 &value) {
    if (value > static_cast<OpcUa_UInt32>(std::numeric_limits<OpcUa_Int32>::max()))
        throw std::runtime_error(SB() << "outgoing data out-of-bounds");
}

inline
void
DataElementUaSdk::logWriteScalar () const
{
    if (isLeaf() && pconnector && debug()) {
        std::cout << pconnector->getRecordName() << ": set outgoing data ("
                  << variantTypeString(outgoingData.type()) << ") to value ";
        if (outgoingData.type() == OpcUaType_String)
            std::cout << "'" << outgoingData.toString().toUtf8() << "'";
        else
            std::cout << outgoingData.toString().toUtf8();
        std::cout << std::endl;
    }
}

void
DataElementUaSdk::writeInt32 (const epicsInt32 &value)
{
    switch (incomingType) {
    case OpcUaType_Int32:
        outgoingData.setInt32(value);
        break;
    case OpcUaType_Boolean:
        if (value == 0)
            outgoingData.setBoolean(false);
        else
            outgoingData.setBoolean(true);
        break;
    case OpcUaType_Byte:
        checkRange<epicsInt32, OpcUa_Byte>(value);
        outgoingData.setByte(static_cast<OpcUa_Byte>(value));
        break;
    case OpcUaType_SByte:
        checkRange<epicsInt32, OpcUa_SByte>(value);
        outgoingData.setSByte(static_cast<OpcUa_SByte>(value));
        break;
    case OpcUaType_UInt16:
        checkRange<epicsInt32, OpcUa_UInt16>(value);
        outgoingData.setUInt16(static_cast<OpcUa_UInt16>(value));
        break;
    case OpcUaType_Int16:
        checkRange<epicsInt32, OpcUa_Int16>(value);
        outgoingData.setInt16(static_cast<OpcUa_Int16>(value));
        break;
    case OpcUaType_UInt32:
        checkRange<epicsInt32, OpcUa_UInt32>(value);
        outgoingData.setUInt32(static_cast<OpcUa_UInt32>(value));
        break;
    case OpcUaType_UInt64:
        outgoingData.setUInt64(static_cast<OpcUa_UInt64>(value));
        break;
    case OpcUaType_Int64:
        outgoingData.setInt64(static_cast<OpcUa_Int64>(value));
        break;
    case OpcUaType_Float:
        outgoingData.setFloat(static_cast<OpcUa_Float>(value));
        break;
    case OpcUaType_Double:
        outgoingData.setDouble(static_cast<OpcUa_Double>(value));
        break;
    case OpcUaType_String:
        outgoingData.setString(static_cast<UaString>(std::to_string(value).c_str()));
        break;
    default:
        throw std::runtime_error(SB() << "unsupported conversion for outgoing data");
    }

    logWriteScalar();
}

void
DataElementUaSdk::writeUInt32 (const epicsUInt32 &value)
{
    switch (incomingType) {
    case OpcUaType_UInt32:
        outgoingData.setUInt32(static_cast<OpcUa_UInt32>(value));
        break;
    case OpcUaType_Boolean:
        if (value == 0)
            outgoingData.setBoolean(false);
        else
            outgoingData.setBoolean(true);
        break;
    case OpcUaType_Byte:
        checkRange<epicsUInt32, OpcUa_Byte>(value);
        outgoingData.setByte(static_cast<OpcUa_Byte>(value));
        break;
    case OpcUaType_SByte:
        checkRange<epicsUInt32, OpcUa_SByte>(value);
        outgoingData.setSByte(static_cast<OpcUa_SByte>(value));
        break;
    case OpcUaType_UInt16:
        checkRange<epicsUInt32, OpcUa_UInt16>(value);
        outgoingData.setUInt16(static_cast<OpcUa_UInt16>(value));
        break;
    case OpcUaType_Int16:
        checkRange<epicsUInt32, OpcUa_Int16>(value);
        outgoingData.setInt16(static_cast<OpcUa_Int16>(value));
        break;
    case OpcUaType_Int32:
        checkRange<epicsUInt32, OpcUa_Int32>(value);
        outgoingData.setInt32(static_cast<OpcUa_Int32>(value));
        break;
    case OpcUaType_UInt64:
        outgoingData.setUInt64(static_cast<OpcUa_UInt64>(value));
        break;
    case OpcUaType_Int64:
        outgoingData.setInt64(static_cast<OpcUa_Int64>(value));
        break;
    case OpcUaType_Float:
        outgoingData.setFloat(static_cast<OpcUa_Float>(value));
        break;
    case OpcUaType_Double:
        outgoingData.setDouble(static_cast<OpcUa_Double>(value));
        break;
    case OpcUaType_String:
        outgoingData.setString(static_cast<UaString>(std::to_string(value).c_str()));
        break;
    default:
        throw std::runtime_error(SB() << "unsupported conversion for outgoing data");
    }

    logWriteScalar();
}

void
DataElementUaSdk::writeInt64 (const epicsInt64 &value)
{
    switch (incomingType) {
    case OpcUaType_Int64:
        outgoingData.setInt64(value);
        break;
    case OpcUaType_Boolean:
        if (value == 0)
            outgoingData.setBoolean(false);
        else
            outgoingData.setBoolean(true);
        break;
    case OpcUaType_Byte:
        checkRange<epicsInt64, OpcUa_Byte>(value);
        outgoingData.setByte(static_cast<OpcUa_Byte>(value));
        break;
    case OpcUaType_SByte:
        checkRange<epicsInt64, OpcUa_SByte>(value);
        outgoingData.setSByte(static_cast<OpcUa_SByte>(value));
        break;
    case OpcUaType_UInt16:
        checkRange<epicsInt64, OpcUa_UInt16>(value);
        outgoingData.setUInt16(static_cast<OpcUa_UInt16>(value));
        break;
    case OpcUaType_Int16:
        checkRange<epicsInt64, OpcUa_Int16>(value);
        outgoingData.setInt16(static_cast<OpcUa_Int16>(value));
        break;
    case OpcUaType_UInt32:
        checkRange<epicsInt64, OpcUa_UInt32>(value);
        outgoingData.setUInt32(static_cast<OpcUa_UInt32>(value));
        break;
    case OpcUaType_Int32:
        outgoingData.setInt32(static_cast<OpcUa_Int32>(value));
        break;
    case OpcUaType_UInt64:
        outgoingData.setUInt64(static_cast<OpcUa_UInt64>(value));
        break;
    case OpcUaType_Float:
        outgoingData.setFloat(static_cast<OpcUa_Float>(value));
        break;
    case OpcUaType_Double:
        outgoingData.setDouble(static_cast<OpcUa_Double>(value));
        break;
    case OpcUaType_String:
        outgoingData.setString(static_cast<UaString>(std::to_string(value).c_str()));
        break;
    default:
        throw std::runtime_error(SB() << "unsupported conversion for outgoing data");
    }

    logWriteScalar();
}

void
DataElementUaSdk::writeFloat64 (const epicsFloat64 &value)
{
    switch (incomingType) {
    case OpcUaType_Double:
        outgoingData.setDouble(static_cast<OpcUa_Double>(value));
        break;
    case OpcUaType_Boolean:
        if (value == 0.)
            outgoingData.setBoolean(false);
        else
            outgoingData.setBoolean(true);
        break;
    case OpcUaType_Byte:
        checkRange<epicsFloat64, OpcUa_Byte>(value);
        outgoingData.setByte(static_cast<OpcUa_Byte>(value));
        break;
    case OpcUaType_SByte:
        checkRange<epicsFloat64, OpcUa_SByte>(value);
        outgoingData.setSByte(static_cast<OpcUa_SByte>(value));
        break;
    case OpcUaType_UInt16:
        checkRange<epicsFloat64, OpcUa_UInt16>(value);
        outgoingData.setUInt16(static_cast<OpcUa_UInt16>(value));
        break;
    case OpcUaType_Int16:
        checkRange<epicsFloat64, OpcUa_Int16>(value);
        outgoingData.setInt16(static_cast<OpcUa_Int16>(value));
        break;
    case OpcUaType_UInt32:
        checkRange<epicsFloat64, OpcUa_UInt32>(value);
        outgoingData.setUInt32(static_cast<OpcUa_UInt32>(value));
        break;
    case OpcUaType_Int32:
        checkRange<epicsFloat64, OpcUa_Int32>(value);
        outgoingData.setInt32(static_cast<OpcUa_Int32>(value));
        break;
    case OpcUaType_UInt64:
        checkRange<epicsFloat64, OpcUa_UInt64>(value);
        outgoingData.setUInt64(static_cast<OpcUa_UInt64>(value));
        break;
    case OpcUaType_Int64:
        checkRange<epicsFloat64, OpcUa_Int64>(value);
        outgoingData.setInt64(static_cast<OpcUa_Int64>(value));
        break;
    case OpcUaType_Float:
        checkRange<epicsFloat64, OpcUa_Float>(value);
        outgoingData.setFloat(static_cast<OpcUa_Float>(value));
        break;
    case OpcUaType_String:
        outgoingData.setString(static_cast<UaString>(std::to_string(value).c_str()));
        break;
    default:
        throw std::runtime_error(SB() << "unsupported conversion for outgoing data");
    }

    logWriteScalar();
}

void
DataElementUaSdk::writeCString(const char *value, const size_t num)
{
    long l;
    unsigned long ul;
    double d;

    switch (incomingType) {
    case OpcUaType_String:
        outgoingData.setString(static_cast<UaString>(value));
        break;
    case OpcUaType_Boolean:
        if (strchr("YyTt1", *value))
            outgoingData.setBoolean(true);
        else
            outgoingData.setBoolean(false);
        break;
    case OpcUaType_Byte:
        ul = strtoul(value, nullptr, 0);
        checkRange<unsigned long, OpcUa_Byte>(ul);
        outgoingData.setByte(static_cast<OpcUa_Byte>(ul));
        break;
    case OpcUaType_SByte:
        l = strtol(value, nullptr, 0);
        checkRange<long, OpcUa_SByte>(l);
        outgoingData.setSByte(static_cast<OpcUa_SByte>(l));
        break;
    case OpcUaType_UInt16:
        ul = strtoul(value, nullptr, 0);
        checkRange<unsigned long, OpcUa_UInt16>(ul);
        outgoingData.setUInt16(static_cast<OpcUa_UInt16>(ul));
        break;
    case OpcUaType_Int16:
        l = strtol(value, nullptr, 0);
        checkRange<long, OpcUa_Int16>(l);
        outgoingData.setInt16(static_cast<OpcUa_Int16>(l));
        break;
    case OpcUaType_UInt32:
        ul = strtoul(value, nullptr, 0);
        checkRange<unsigned long, OpcUa_UInt32>(ul);
        outgoingData.setUInt32(static_cast<OpcUa_UInt32>(ul));
        break;
    case OpcUaType_Int32:
        l = strtol(value, nullptr, 0);
        checkRange<long, OpcUa_Int32>(l);
        outgoingData.setInt32(static_cast<OpcUa_Int32>(l));
        break;
    case OpcUaType_UInt64:
        ul = strtoul(value, nullptr, 0);
        checkRange<unsigned long, OpcUa_UInt64>(ul);
        outgoingData.setUInt64(static_cast<OpcUa_UInt64>(ul));
        break;
    case OpcUaType_Int64:
        l = strtol(value, nullptr, 0);
        checkRange<long, OpcUa_Int64>(l);
        outgoingData.setInt64(static_cast<OpcUa_Int64>(l));
        break;
    case OpcUaType_Float:
        d = strtod(value, nullptr);
        checkRange<double, OpcUa_Float>(d);
        outgoingData.setFloat(static_cast<OpcUa_Float>(d));
        break;
    case OpcUaType_Double:
        d = strtod(value, nullptr);
        outgoingData.setDouble(static_cast<OpcUa_Double>(d));
        break;
    default:
        throw std::runtime_error(SB() << "unsupported conversion for outgoing data");
    }

    logWriteScalar();
}

void
DataElementUaSdk::checkWriteArray (OpcUa_BuiltInType expectedType, const std::string &targetTypeName) const
{
    if (!incomingIsArray)
        throw std::runtime_error(SB() << "OPC UA data is not an array");
    if (incomingType != expectedType)
        throw std::runtime_error(SB() << "OPC UA array data type (" << variantTypeString(incomingType) << ")"
                                 << " does not match expected type (" << variantTypeString(expectedType) << ")"
                                 << " for EPICS array type (" << targetTypeName << ")");
}

inline
void
DataElementUaSdk::logWriteArray (const epicsUInt32 targetSize, const std::string &targetTypeName) const
{
    if (isLeaf() && debug()) {
        std::cout << pconnector->getRecordName() << ": writing array of "
                  << targetTypeName << "[" << targetSize << "] as "
                  << variantTypeString(outgoingData.type()) << "["<< outgoingData.arraySize() << "]"
                  << std::endl;
    }
}


// The array methods must cast their arguments as the UA SDK API uses non-const parameters

void
DataElementUaSdk::writeArrayInt8 (const epicsInt8 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsInt8";
    checkWriteArray(OpcUaType_SByte, etype);

    OpcUa_SByte *val = const_cast<OpcUa_SByte *>(reinterpret_cast<const OpcUa_SByte *>(value));
    OpcUa_Int32 size = static_cast<OpcUa_Int32>(num);
    UaSByteArray arr(size, val);
    outgoingData.setSByteArray(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::writeArrayUInt8 (const epicsUInt8 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsUInt8";
    checkWriteArray(OpcUaType_Byte, etype);

    const char *val = reinterpret_cast<const char *>(value);
    int size = static_cast<int>(num);
    UaByteArray arr(val, size);
    outgoingData.setByteArray(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::writeArrayInt16 (const epicsInt16 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsInt16";
    checkWriteArray(OpcUaType_Int16, etype);

    OpcUa_Int16 *val = const_cast<OpcUa_Int16 *>(value);
    OpcUa_Int32 size = static_cast<OpcUa_Int32>(num);
    UaInt16Array arr(size, val);
    outgoingData.setInt16Array(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::writeArrayUInt16 (const epicsUInt16 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsUInt16";
    checkWriteArray(OpcUaType_UInt16, etype);

    OpcUa_UInt16 *val = const_cast<OpcUa_UInt16 *>(value);
    OpcUa_Int32 size = static_cast<OpcUa_Int32>(num);
    UaUInt16Array arr(size, val);
    outgoingData.setUInt16Array(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::writeArrayInt32 (const epicsInt32 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsInt32";
    checkWriteArray(OpcUaType_Int32, etype);

    OpcUa_Int32 *val = const_cast<OpcUa_Int32 *>(reinterpret_cast<const OpcUa_Int32 *>(value));
    OpcUa_Int32 size = static_cast<OpcUa_Int32>(num);
    UaInt32Array arr(size, val);
    outgoingData.setInt32Array(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::writeArrayUInt32 (const epicsUInt32 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsUInt32";
    checkWriteArray(OpcUaType_UInt32, etype);

    OpcUa_UInt32 *val = const_cast<OpcUa_UInt32 *>(reinterpret_cast<const OpcUa_UInt32 *>(value));
    OpcUa_Int32 size = static_cast<OpcUa_Int32>(num);
    UaUInt32Array arr(size, val);
    outgoingData.setUInt32Array(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::writeArrayInt64 (const epicsInt64 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsInt64";

    checkWriteArray(OpcUaType_Int64, etype);

    OpcUa_Int64 *val = const_cast<OpcUa_Int64 *>(reinterpret_cast<const OpcUa_Int64 *>(value));
    OpcUa_Int32 size = static_cast<OpcUa_Int32>(num);
    UaInt64Array arr(size, val);
    outgoingData.setInt64Array(arr, OpcUa_True);
}

void
DataElementUaSdk::writeArrayUInt64 (const epicsUInt64 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsUInt64";
    checkWriteArray(OpcUaType_UInt64, etype);

    OpcUa_UInt64 *val = const_cast<OpcUa_UInt64 *>(reinterpret_cast<const OpcUa_UInt64 *>(value));
    OpcUa_Int32 size = static_cast<OpcUa_Int32>(num);
    UaUInt64Array arr(size, val);
    outgoingData.setUInt64Array(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::writeArrayFloat32 (const epicsFloat32 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsFloat32";
    checkWriteArray(OpcUaType_Float, etype);

    OpcUa_Float *val = const_cast<OpcUa_Float *>(value);
    OpcUa_Int32 size = static_cast<OpcUa_Int32>(num);
    UaFloatArray arr(size, val);
    outgoingData.setFloatArray(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::writeArrayFloat64 (const epicsFloat64 *value, const epicsUInt32 num)
{
    const std::string etype = "epicsFloat64";
    checkWriteArray(OpcUaType_Double, etype);

    OpcUa_Double *val = const_cast<OpcUa_Double *>(value);
    OpcUa_Int32 size = static_cast<OpcUa_Int32>(num);
    UaDoubleArray arr(size, val);
    outgoingData.setDoubleArray(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::writeArrayOldString (const epicsOldString *value, const epicsUInt32 num)
{
    const std::string etype = "epicsOldString";
    checkWriteArray(OpcUaType_String, etype);

    UaStringArray arr;
    arr.create(static_cast<OpcUa_UInt32>(num));
    for (epicsUInt32 i = 0; i < num; i++) {
        // add zero termination if necessary
        char val[MAX_STRING_SIZE+1] = { 0 };
        const char *pval;
        if (memchr(value[i], '\0', MAX_STRING_SIZE) == nullptr) {
            strncpy(val, value[i], MAX_STRING_SIZE);
            pval = val;
        } else {
            pval = value[i];
        }
        UaString(pval).copyTo(&arr[i]);
    }
    outgoingData.setStringArray(arr, OpcUa_True);

    logWriteArray(num, etype);
}

void
DataElementUaSdk::requestRecordProcessing (const ProcessReason reason) const
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
