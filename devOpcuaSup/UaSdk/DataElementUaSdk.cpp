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

#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <cstdlib>

#include <uadatetime.h>
#include <uaextensionobject.h>
#include <uaarraytemplates.h>
#include <opcua_builtintypes.h>
#include <statuscode.h>

#include <errlog.h>
#include <epicsTime.h>
#include <alarm.h>

#include "ItemUaSdk.h"
#include "DataElementUaSdk.h"
#include "UpdateQueue.h"
#include "RecordConnector.h"

namespace DevOpcua {

DataElementUaSdk::DataElementUaSdk (const std::string &name,
                                    ItemUaSdk *item,
                                    RecordConnector *pconnector)
    : DataElement(pconnector, name)
    , pitem(item)
    , mapped(false)
    , incomingQueue(pconnector->plinkinfo->clientQueueSize, pconnector->plinkinfo->discardOldest)
{}

DataElementUaSdk::DataElementUaSdk (const std::string &name,
                                    ItemUaSdk *item,
                                    std::weak_ptr<DataElementUaSdk> child)
    : DataElement(name)
    , pitem(item)
    , mapped(false)
    , incomingQueue(0ul)
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
                  << " type=" << variantTypeString(incomingData.type())
                  << " timestamp=" << (pconnector->plinkinfo->useServerTimestamp ? "server" : "source")
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

// Getting the timestamp and status information from the Item assumes that only one thread
// is pushing data into the Item's DataElement structure at any time.
void
DataElementUaSdk::setIncomingData (const UaVariant &value, ProcessReason reason)
{
    if (isLeaf()) {
        if (reason == ProcessReason::readComplete || pconnector->plinkinfo->monitor) {
            Guard(pconnector->lock);
            bool wasFirst = false;
            // Make a copy of this element and cache it
            incomingData = value;
            // Make another copy of the value for this element and put it on the queue
            UpdateUaSdk *u(new UpdateUaSdk(getIncomingTimeStamp(), reason, value, getIncomingReadStatus()));
            incomingQueue.pushUpdate(std::shared_ptr<UpdateUaSdk>(u), &wasFirst);
            if (debug() >= 5)
                std::cout << "Element " << name << " set data ("
                          << processReasonString(reason)
                          << ") for record " << pconnector->getRecordName()
                          << " (queue use " << incomingQueue.size()
                          << "/" << incomingQueue.capacity() << ")" << std::endl;
            if (wasFirst)
                pconnector->requestRecordProcessing(reason);
        }
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
        UpdateUaSdk *u(new UpdateUaSdk(getIncomingTimeStamp(), reason));
        incomingQueue.pushUpdate(std::shared_ptr<UpdateUaSdk>(u), &wasFirst);
        if (debug() >= 5)
            std::cout << "Element " << name << " set event ("
                      << processReasonString(reason)
                      << ") for record " << pconnector->getRecordName()
                      << " (queue use " << incomingQueue.size()
                      << "/" << incomingQueue.capacity() << ")" << std::endl;
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
DataElementUaSdk::dbgReadScalar (const UpdateUaSdk *upd,
                                 const std::string &targetTypeName,
                                 const size_t targetSize) const
{
    if (isLeaf() && debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        ProcessReason reason = upd->getType();

        std::cout << pconnector->getRecordName() << ": ";
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete) {
            std::cout << "(" << ( pconnector->plinkinfo->useServerTimestamp ? "server" : "device")
                      << " time " << time_buf << ") read " << processReasonString(reason) << " ("
                      << UaStatus(upd->getStatus()).toString().toUtf8() << ") ";
            UaVariant &data = upd->getData();
            if (data.type() == OpcUaType_String)
                std::cout << "'" << data.toString().toUtf8() << "'";
            else
                std::cout << data.toString().toUtf8();
            std::cout << " (" << variantTypeString(data.type()) << ")"
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
DataElementUaSdk::readScalar (epicsInt32 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              epicsOldString *statusText)
{
    return readScalar<epicsInt32, OpcUa_Int32>(value, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readScalar (epicsInt64 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              epicsOldString *statusText)
{
    return readScalar<epicsInt64, OpcUa_Int64>(value, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readScalar (epicsUInt32 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              epicsOldString *statusText)
{
    return readScalar<epicsUInt32, OpcUa_UInt32>(value, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readScalar (epicsFloat64 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              epicsOldString *statusText)
{
    return readScalar<epicsFloat64, OpcUa_Double>(value, prec, nextReason, statusCode, statusText);
}

// CString type needs specialization
long
DataElementUaSdk::readScalar (char *value, const size_t num,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              epicsOldString *statusText)
{
    long ret = 0;

    if (incomingQueue.empty()) {
        errlogPrintf("%s : incoming data queue empty\n", prec->name);
        return 1;
    }

    ProcessReason nReason;
    std::shared_ptr<UpdateUaSdk> upd = incomingQueue.popUpdate(&nReason);
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
        OpcUa_StatusCode stat = upd->getStatus();
        if (OpcUa_IsNotGood(stat)) {
            // No valid OPC UA value
            (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            ret = 1;
        } else {
            // Valid OPC UA value, so copy over
            if (OpcUa_IsUncertain(stat)) {
                (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
            }
            if (num) {
                strncpy(value, upd->getData().toString().toUtf8(), num);
                value[num-1] = '\0';
                prec->udf = false;
            }
        }
        if (statusCode) *statusCode = stat;
        if (statusText) strncpy(*statusText, UaStatus(stat).toString().toUtf8(), MAX_STRING_SIZE);
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
DataElementUaSdk::dbgReadArray (const UpdateUaSdk *upd,
                                const epicsUInt32 targetSize,
                                const std::string &targetTypeName) const
{
    if (isLeaf() && debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        ProcessReason reason = upd->getType();

        std::cout << pconnector->getRecordName() << ": ";
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete) {
            std::cout << "(" << ( pconnector->plinkinfo->useServerTimestamp ? "server" : "device")
                      << " time " << time_buf << ") read " << processReasonString(reason) << " ("
                      << UaStatus(upd->getStatus()).toString().toUtf8() << ") ";
            UaVariant &data = upd->getData();
            std::cout << " array of " << variantTypeString(data.type())
                      << "[" << upd->getData().arraySize() << "]"
                      << " into " << targetTypeName << "[" << targetSize << "]";
        } else {
            std::cout << "(client time "<< time_buf << ") " << processReasonString(reason);
        }
        std::cout << " --- remaining queue " << incomingQueue.size()
                  << "/" << incomingQueue.capacity() << std::endl;
    }
}

// Specialization for epicsOldString / OpcUa_String
// CAVEAT: changes in the template (in DataElementUaSdk.h) must be reflected here
template<>
long
DataElementUaSdk::readArray<epicsOldString, UaStringArray> (epicsOldString *value, const epicsUInt32 num,
                                                            epicsUInt32 *numRead,
                                                            OpcUa_BuiltInType expectedType,
                                                            dbCommon *prec,
                                                            ProcessReason *nextReason,
                                                            epicsUInt32 *statusCode,
                                                            epicsOldString *statusText)
{
    long ret = 0;
    epicsUInt32 elemsWritten = 0;

    if (incomingQueue.empty()) {
        errlogPrintf("%s : incoming data queue empty\n", prec->name);
        *numRead = 0;
        return 1;
    }

    ProcessReason nReason;
    std::shared_ptr<UpdateUaSdk> upd = incomingQueue.popUpdate(&nReason);
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
        OpcUa_StatusCode stat = upd->getStatus();
        if (OpcUa_IsNotGood(stat)) {
            // No valid OPC UA value
            (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            ret = 1;
        } else {
            // Valid OPC UA value, so try to convert
            UaVariant &data = upd->getData();
            if (!data.isArray()) {
                errlogPrintf("%s : incoming data is not an array\n", prec->name);
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else if (data.type() != expectedType) {
                errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                             prec->name, variantTypeString(data.type()), epicsTypeString(*value));
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                if (OpcUa_IsUncertain(stat)) {
                    (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                }
                UaStringArray arr;
                UaVariant_to(upd->getData(), arr);
                elemsWritten = num < arr.length() ? num : arr.length();
                for (epicsUInt32 i = 0; i < elemsWritten; i++)
                    strncpy(value[i], UaString(arr[i]).toUtf8(), MAX_STRING_SIZE);
                prec->udf = false;
            }
        }
        if (statusCode) *statusCode = stat;
        if (statusText) strncpy(*statusText, UaStatus(stat).toString().toUtf8(), MAX_STRING_SIZE);
        break;
    }
    default:
        break;
    }

    prec->time = upd->getTimeStamp();
    if (nextReason) *nextReason = nReason;
    *numRead = elemsWritten;
    return ret;
}

// Specialization for epicsUInt8 / OpcUa_Byte
//   (needed because UaByteArray API is different from all other UaXxxArray classes)
// CAVEAT: changes in the template (in DataElementUaSdk.h) must be reflected here
template<>
long
DataElementUaSdk::readArray<epicsUInt8, UaByteArray> (epicsUInt8 *value, const epicsUInt32 num,
                                                      epicsUInt32 *numRead,
                                                      OpcUa_BuiltInType expectedType,
                                                      dbCommon *prec,
                                                      ProcessReason *nextReason,
                                                      epicsUInt32 *statusCode,
                                                      epicsOldString *statusText)
{
    long ret = 0;
    epicsUInt32 elemsWritten = 0;

    if (incomingQueue.empty()) {
        errlogPrintf("%s : incoming data queue empty\n", prec->name);
        *numRead = 0;
        return 1;
    }

    ProcessReason nReason;
    std::shared_ptr<UpdateUaSdk> upd = incomingQueue.popUpdate(&nReason);
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
        OpcUa_StatusCode stat = upd->getStatus();
        if (OpcUa_IsNotGood(stat)) {
            // No valid OPC UA value
            (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            ret = 1;
        } else {
            // Valid OPC UA value, so try to convert
            UaVariant &data = upd->getData();
            if (!data.isArray()) {
                errlogPrintf("%s : incoming data is not an array\n", prec->name);
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else if (data.type() != expectedType) {
                errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                             prec->name, variantTypeString(data.type()), epicsTypeString(*value));
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                if (OpcUa_IsUncertain(stat)) {
                    (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                }
                UaByteArray arr;
                UaVariant_to(upd->getData(), arr);
                elemsWritten = static_cast<epicsUInt32>(arr.size());
                if (num < elemsWritten) elemsWritten = num;
                memcpy(value, arr.data(), sizeof(epicsUInt8) * elemsWritten);
                prec->udf = false;
            }
        }
        if (statusCode) *statusCode = stat;
        if (statusText) strncpy(*statusText, UaStatus(stat).toString().toUtf8(), MAX_STRING_SIZE);
        break;
    }
    default:
        break;
    }

    prec->time = upd->getTimeStamp();
    if (nextReason) *nextReason = nReason;
    *numRead = elemsWritten;
    return ret;
}

long
DataElementUaSdk::readArray (epicsInt8 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsInt8, UaSByteArray>(value, num, numRead, OpcUaType_SByte, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsUInt8 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsUInt8, UaByteArray>(value, num, numRead, OpcUaType_Byte, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsInt16 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsInt16, UaInt16Array>(value, num, numRead, OpcUaType_Int16, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsUInt16 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsUInt16, UaUInt16Array>(value, num, numRead, OpcUaType_UInt16, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsInt32 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsInt32, UaInt32Array>(value, num, numRead, OpcUaType_Int32, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsUInt32 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsUInt32, UaUInt32Array>(value, num, numRead, OpcUaType_UInt32, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsInt64 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsInt64, UaInt64Array>(value, num, numRead, OpcUaType_Int64, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsUInt64 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsUInt64, UaUInt64Array>(value, num, numRead, OpcUaType_UInt64, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsFloat32 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsFloat32, UaFloatArray>(value, num, numRead, OpcUaType_Float, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsFloat64 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsFloat64, UaDoubleArray>(value, num, numRead, OpcUaType_Double, prec, nextReason, statusCode, statusText);
}

long
DataElementUaSdk::readArray (epicsOldString *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             epicsOldString *statusText)
{
    return readArray<epicsOldString, UaStringArray>(value, num, numRead, OpcUaType_String, prec, nextReason, statusCode, statusText);
}

inline
void
DataElementUaSdk::dbgWriteScalar () const
{
    if (isLeaf() && debug()) {
        std::cout << pconnector->getRecordName() << ": set outgoing data ("
                  << variantTypeString(outgoingData.type()) << ") to value ";
        if (outgoingData.type() == OpcUaType_String)
            std::cout << "'" << outgoingData.toString().toUtf8() << "'";
        else
            std::cout << outgoingData.toString().toUtf8();
        std::cout << std::endl;
    }
}

long
DataElementUaSdk::writeScalar (const epicsInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsInt32>(value, prec);
}

long
DataElementUaSdk::writeScalar (const epicsUInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsUInt32>(value, prec);
}

long
DataElementUaSdk::writeScalar (const epicsInt64 &value, dbCommon *prec)
{
    return writeScalar<epicsInt64>(value, prec);
}

long
DataElementUaSdk::writeScalar (const epicsFloat64 &value, dbCommon *prec)
{
    return writeScalar<epicsFloat64>(value, prec);
}

long
DataElementUaSdk::writeScalar (const char *value, dbCommon *prec)
{
    long ret = 0;
    long l;
    unsigned long ul;
    double d;

    switch (incomingData.type()) {
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
        if (isWithinRange<OpcUa_Byte>(ul)) {
            outgoingData.setByte(static_cast<OpcUa_Byte>(ul));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_SByte:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<OpcUa_SByte>(l)) {
            outgoingData.setSByte(static_cast<OpcUa_SByte>(l));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_UInt16:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<OpcUa_UInt16>(ul)) {
            outgoingData.setUInt16(static_cast<OpcUa_UInt16>(ul));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Int16:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<OpcUa_Int16>(l)) {
            outgoingData.setInt16(static_cast<OpcUa_Int16>(l));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_UInt32:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<OpcUa_UInt32>(ul)) {
            outgoingData.setUInt32(static_cast<OpcUa_UInt32>(ul));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Int32:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<OpcUa_Int32>(l)) {
            outgoingData.setInt32(static_cast<OpcUa_Int32>(l));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_UInt64:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<OpcUa_UInt64>(ul)) {
            outgoingData.setUInt64(static_cast<OpcUa_UInt64>(ul));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Int64:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<long, OpcUa_Int64>(l)) {
            outgoingData.setInt64(static_cast<OpcUa_Int64>(l));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Float:
        d = strtod(value, nullptr);
        if (isWithinRange<OpcUa_Float>(d)) {
            outgoingData.setFloat(static_cast<OpcUa_Float>(d));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Double:
        d = strtod(value, nullptr);
        outgoingData.setDouble(static_cast<OpcUa_Double>(d));
        break;
    default:
        errlogPrintf("%s : unsupported conversion for outgoing data\n",
                     prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
    }

    dbgWriteScalar();
    return ret;
}

inline
void
DataElementUaSdk::dbgWriteArray (const epicsUInt32 targetSize, const std::string &targetTypeName) const
{
    if (isLeaf() && debug()) {
        std::cout << pconnector->getRecordName() << ": writing array of "
                  << targetTypeName << "[" << targetSize << "] as "
                  << variantTypeString(outgoingData.type()) << "["<< outgoingData.arraySize() << "]"
                  << std::endl;
    }
}

// Specialization for epicsOldString / OpcUa_String
// CAVEAT: changes in the template (in DataElementUaSdk.h) must be reflected here
template<>
long
DataElementUaSdk::writeArray<epicsOldString, UaStringArray, OpcUa_String> (const epicsOldString *value, const epicsUInt32 num,
                                                                           OpcUa_BuiltInType targetType,
                                                                           dbCommon *prec)
{
    long ret = 0;

    if (!incomingData.isArray()) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else if (incomingData.type() != targetType) {
        errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                     prec->name,
                     variantTypeString(incomingData.type()),
                     variantTypeString(targetType),
                     epicsTypeString(*value));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
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
        UaVariant_set(outgoingData, arr);

        dbgWriteArray(num, epicsTypeString(*value));
    }
    return ret;
}

// Specialization for epicsUInt8 / OpcUa_Byte
//   (needed because UaByteArray API is different from all other UaXxxArray classes)
// CAVEAT: changes in the template (in DataElementUaSdk.h) must be reflected here
template<>
long
DataElementUaSdk::writeArray<epicsUInt8, UaByteArray, OpcUa_Byte> (const epicsUInt8 *value, const epicsUInt32 num,
                                                                   OpcUa_BuiltInType targetType,
                                                                   dbCommon *prec)
{
    long ret = 0;

    if (!incomingData.isArray()) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else if (incomingData.type() != targetType) {
        errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                     prec->name,
                     variantTypeString(incomingData.type()),
                     variantTypeString(targetType),
                     epicsTypeString(*value));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
        UaByteArray arr(reinterpret_cast<const char *>(value), static_cast<OpcUa_Int32>(num));
        UaVariant_set(outgoingData, arr);

        dbgWriteArray(num, epicsTypeString(*value));
    }
    return ret;
}

long
DataElementUaSdk::writeArray (const epicsInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt8, UaSByteArray, OpcUa_SByte>(value, num, OpcUaType_SByte, prec);
}

long
DataElementUaSdk::writeArray (const epicsUInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt8, UaByteArray, OpcUa_Byte>(value, num, OpcUaType_Byte, prec);
}

long
DataElementUaSdk::writeArray (const epicsInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt16, UaInt16Array, OpcUa_Int16>(value, num, OpcUaType_Int16, prec);
}

long
DataElementUaSdk::writeArray (const epicsUInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt16, UaUInt16Array, OpcUa_UInt16>(value, num, OpcUaType_UInt16, prec);
}

long
DataElementUaSdk::writeArray (const epicsInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt32, UaInt32Array, OpcUa_Int32>(value, num, OpcUaType_Int32, prec);
}

long
DataElementUaSdk::writeArray (const epicsUInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt32, UaUInt32Array, OpcUa_UInt32>(value, num, OpcUaType_UInt32, prec);
}

long
DataElementUaSdk::writeArray (const epicsInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt64, UaInt64Array, OpcUa_Int64>(value, num, OpcUaType_Int64, prec);
}

long
DataElementUaSdk::writeArray (const epicsUInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt64, UaUInt64Array, OpcUa_UInt64>(value, num, OpcUaType_UInt64, prec);
}

long
DataElementUaSdk::writeArray (const epicsFloat32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat32, UaFloatArray, OpcUa_Float>(value, num, OpcUaType_Float, prec);
}

long
DataElementUaSdk::writeArray (const epicsFloat64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat64, UaDoubleArray, OpcUa_Double>(value, num, OpcUaType_Double, prec);
}

long
DataElementUaSdk::writeArray (const epicsOldString *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsOldString, UaStringArray, OpcUa_String>(value, num, OpcUaType_String, prec);
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
