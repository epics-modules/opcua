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

#include "DataElementUaSdkLeaf.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <opcua_builtintypes.h>
#include <statuscode.h>
#include <uaarraytemplates.h>
#include <uadatetime.h>
#include <uaenumdefinition.h>
#include <uaextensionobject.h>
#include <uagenericunionvalue.h>

#include <alarm.h>
#include <epicsTime.h>
#include <errlog.h>

#include "DataElementUaSdk.h"
#include "DataElementUaSdkNode.h"
#include "ItemUaSdk.h"
#include "RecordConnector.h"

namespace DevOpcua
{

DataElementUaSdkLeaf::DataElementUaSdkLeaf (const std::string &name,
                                            class ItemUaSdk *pitem,
                                            class RecordConnector *pconnector)
    : DataElementUaSdk(name, pitem)
    , incomingQueue(pconnector->plinkinfo->clientQueueSize, pconnector->plinkinfo->discardOldest)
{}

/* Explicitly implement the destructor here (allows the compiler to place the vtable) */
DataElementUaSdk::~DataElementUaSdk() = default;

/* Specific implementation of DataElement's "factory" method */
void
DataElement::addElementToTree(Item *item, RecordConnector *pconnector, const std::list<std::string> &elementPath)
{
    DataElementUaSdkLeaf::addElementToTree(static_cast<ItemUaSdk *>(item), pconnector, elementPath);
}

void
DataElementUaSdkLeaf::addElementToTree(ItemUaSdk *item,
                                       RecordConnector *pconnector,
                                       const std::list<std::string> &elementPath)
{
    std::string name("[ROOT]");
    if (elementPath.size())
        name = elementPath.back();

    auto leaf = std::make_shared<DataElementUaSdkLeaf>(name, item, pconnector);
    item->dataTree.addLeaf(leaf, elementPath, item);
    // reference from connector after adding to the tree worked
    pconnector->setDataElement(leaf);
    leaf->pconnector = pconnector;
}

void
DataElementUaSdkLeaf::show(const int, const unsigned int indent) const
{
    std::string ind(indent * 2, ' ');
    std::cout << ind;
    std::cout << "leaf=" << name << " record(" << pconnector->getRecordType() << ")=" << pconnector->getRecordName()
              << " type=" << variantTypeString(incomingData.type())
              << " timestamp=" << linkOptionTimestampString(pconnector->plinkinfo->timestamp);
    if (pconnector->plinkinfo->timestamp == LinkOptionTimestamp::data)
        std::cout << "@" << pitem->linkinfo.timestampElement;
    std::cout << " bini=" << linkOptionBiniString(pconnector->plinkinfo->bini)
              << " monitor=" << (pconnector->plinkinfo->monitor ? "y" : "n") << "\n";
}

void
DataElementUaSdkLeaf::setIncomingData (const UaVariant &value,
                                       ProcessReason reason,
                                       const std::string *,
                                       const UaNodeId *typeId)
{
    incomingData = value;

    if (typeId && pconnector->state() == ConnectionStatus::initialRead) {
        delete enumChoices;
        enumChoices = pitem->session->getEnumChoices(typeId);
    }
    if ((pconnector->state() == ConnectionStatus::initialRead
         && (reason == ProcessReason::readComplete || reason == ProcessReason::readFailure))
        || (pconnector->state() == ConnectionStatus::up)) {
        Guard(pconnector->lock);
        bool wasFirst = false;
        // Make a copy of the value for this element and put it on the queue
        UpdateUaSdk *u(new UpdateUaSdk(getIncomingTimeStamp(), reason, value, getIncomingReadStatus()));
        incomingQueue.pushUpdate(std::shared_ptr<UpdateUaSdk>(u), &wasFirst);
        if (debug() >= 5)
            std::cout << "Element " << name << " set data (" << processReasonString(reason) << ") for record "
                      << pconnector->getRecordName() << " (queue use " << incomingQueue.size() << "/"
                      << incomingQueue.capacity() << ")" << std::endl;
        if (wasFirst)
            pconnector->requestRecordProcessing(reason);
    }
}

void
DataElementUaSdkLeaf::setIncomingEvent (ProcessReason reason)
{
    Guard(pconnector->lock);
    bool wasFirst = false;
    UpdateUaSdk *u(new UpdateUaSdk(getIncomingTimeStamp(), reason));
    incomingQueue.pushUpdate(std::shared_ptr<UpdateUaSdk>(u), &wasFirst);
    if (debug() >= 5)
        std::cout << "Element " << name << " set event (" << processReasonString(reason) << ") for record "
                  << pconnector->getRecordName() << " (queue use " << incomingQueue.size() << "/"
                  << incomingQueue.capacity() << ")" << std::endl;
    if (wasFirst)
        pconnector->requestRecordProcessing(reason);
}

void
DataElementUaSdkLeaf::setState(const ConnectionStatus state)
{
    Guard(pconnector->lock);
    pconnector->setState(state);
}

const UaVariant &
DataElementUaSdkLeaf::getOutgoingData()
{
    return outgoingData;
}

void
DataElementUaSdkLeaf::clearOutgoingData()
{
    outgoingData.clear();
}

void
DataElementUaSdkLeaf::requestRecordProcessing(const ProcessReason reason) const
{
    pconnector->requestRecordProcessing(reason);
}

int
DataElementUaSdkLeaf::debug() const
{
    return pconnector->debug();
}

const epicsTime &
DataElementUaSdkLeaf::getIncomingTimeStamp() const
{
    ProcessReason reason = pitem->getReason();
    if ((reason == ProcessReason::incomingData || reason == ProcessReason::readComplete))
        switch (pconnector->plinkinfo->timestamp) {
        case LinkOptionTimestamp::server:
            return pitem->tsServer;
        case LinkOptionTimestamp::source:
            return pitem->tsSource;
        case LinkOptionTimestamp::data:
            return pitem->tsData;
        }
    return pitem->tsClient;
}

OpcUa_StatusCode
DataElementUaSdkLeaf::getIncomingReadStatus() const
{
    return pitem->getLastStatus().code();
}

OpcUa_StatusCode
DataElementUaSdkLeaf::UaVariant_to(const UaVariant &variant, OpcUa_Int32 &value)
{
    return variant.toInt32(value);
}
OpcUa_StatusCode
DataElementUaSdkLeaf::UaVariant_to(const UaVariant &variant, OpcUa_UInt32 &value)
{
    return variant.toUInt32(value);
}
OpcUa_StatusCode
DataElementUaSdkLeaf::UaVariant_to(const UaVariant &variant, OpcUa_Int64 &value)
{
    return variant.toInt64(value);
}
OpcUa_StatusCode
DataElementUaSdkLeaf::UaVariant_to(const UaVariant &variant, OpcUa_Double &value)
{
    return variant.toDouble(value);
}

void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaBooleanArray &value)
{
    variant.setBoolArray(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaSByteArray &value)
{
    variant.setSByteArray(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaByteArray &value)
{
    variant.setByteArray(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaInt16Array &value)
{
    variant.setInt16Array(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaUInt16Array &value)
{
    variant.setUInt16Array(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaInt32Array &value)
{
    variant.setInt32Array(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaUInt32Array &value)
{
    variant.setUInt32Array(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaInt64Array &value)
{
    variant.setInt64Array(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaUInt64Array &value)
{
    variant.setUInt64Array(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaFloatArray &value)
{
    variant.setFloatArray(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaDoubleArray &value)
{
    variant.setDoubleArray(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaStringArray &value)
{
    variant.setStringArray(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaXmlElementArray &value)
{
    variant.setXmlElementArray(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaLocalizedTextArray &value)
{
    variant.setLocalizedTextArray(value, OpcUa_True);
}
void
DataElementUaSdkLeaf::UaVariant_set(UaVariant &variant, UaQualifiedNameArray &value)
{
    variant.setQualifiedNameArray(value, OpcUa_True);
}

void
DataElementUaSdkLeaf::dbgReadScalar(const UpdateUaSdk *upd,
                                    const std::string &targetTypeName,
                                    const size_t targetSize) const
{
    if (isLeaf() && debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        ProcessReason reason = upd->getType();

        std::cout << pconnector->getRecordName() << ": ";
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete) {
            std::cout << "(" << linkOptionTimestampString(pconnector->plinkinfo->timestamp);
            if (pconnector->plinkinfo->timestamp == LinkOptionTimestamp::data)
                std::cout << "(@" << pconnector->plinkinfo->timestampElement << ")";
            std::cout << " time " << time_buf << ") read " << processReasonString(reason) << " ("
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
            std::cout << "(client time " << time_buf << ") " << processReasonString(reason);
        }
        std::cout << " --- remaining queue " << incomingQueue.size() << "/" << incomingQueue.capacity() << std::endl;
    }
}

long
DataElementUaSdkLeaf::readScalar(epicsInt32 *value,
                                 dbCommon *prec,
                                 ProcessReason *nextReason,
                                 epicsUInt32 *statusCode,
                                 char *statusText,
                                 const epicsUInt32 statusTextLen)
{
    return readScalar<epicsInt32, OpcUa_Int32>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readScalar(epicsInt64 *value,
                                 dbCommon *prec,
                                 ProcessReason *nextReason,
                                 epicsUInt32 *statusCode,
                                 char *statusText,
                                 const epicsUInt32 statusTextLen)
{
    return readScalar<epicsInt64, OpcUa_Int64>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readScalar(epicsUInt32 *value,
                                 dbCommon *prec,
                                 ProcessReason *nextReason,
                                 epicsUInt32 *statusCode,
                                 char *statusText,
                                 const epicsUInt32 statusTextLen)
{
    return readScalar<epicsUInt32, OpcUa_UInt32>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readScalar(epicsFloat64 *value,
                                 dbCommon *prec,
                                 ProcessReason *nextReason,
                                 epicsUInt32 *statusCode,
                                 char *statusText,
                                 const epicsUInt32 statusTextLen)
{
    return readScalar<epicsFloat64, OpcUa_Double>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

// Helper functions to convert between OpcUa_ByteString and hex encoded C string
// We use our own functions instead of UaByteString methods toHex/fromHex because
// they work on UaString, not C string, which would require additional copy.
static int
printByteString(const OpcUa_ByteString &byteString, char *encoded, int len)
{
    static const char hexdigits[] = "0123456789ABCDEF";
    int i, l = 0;
    for (i = 0; i < byteString.Length && l < len - 3; i++) {
        encoded[l++] = hexdigits[byteString.Data[i] >> 4];
        encoded[l++] = hexdigits[byteString.Data[i] & 0xf];
    }
    encoded[l] = 0; // terminate
    return l;
}

// parseByteString accepts hex strings with optional spaces separating units of 1 or multiple of 2 hex digits
static int
parseByteString(OpcUa_ByteString &byteString, const char *encoded, int len)
{
    OpcUa_ByteString_Clear(&byteString);
    byteString.Data
        = static_cast<OpcUa_Byte *>(OpcUa_Alloc((len + 1) / 2)); // can never be longer, shorter does not hurt
    if (!byteString.Data)
        return -1;
    int l = 0;
    bool firstDigit = true;
    while (len--) {
        OpcUa_Byte c, b;
        c = *encoded++;
        if (!c)
            break;
        if (isblank(c)) {
            firstDigit = true;
            continue;
        }
        if (!isxdigit(c)) {
            // invalid character
            OpcUa_ByteString_Clear(&byteString);
            return -1;
        }
        b = c - (isdigit(c) ? '0' : isupper(c) ? 'A' - 10 : 'a' - 10);
        if (len && isxdigit(c = *encoded)) {
            firstDigit = false;
            encoded++;
            len--;
            b <<= 4;
            b |= c - (isdigit(c) ? '0' : isupper(c) ? 'A' - 10 : 'a' - 10);
        } else {
            if (!firstDigit || (c && !isblank(c) && !isxdigit(c))) {
                // 1 is the only odd number of digits allowed
                // because otherwise where is the byte border? 12|3 or 1|23 ?
                OpcUa_ByteString_Clear(&byteString);
                return -1;
            }
        }
        byteString.Data[l++] = b;
    }
    byteString.Length = l;
    return l;
}

// CString type needs specialization
long
DataElementUaSdkLeaf::readScalar(char *value,
                                 const epicsUInt32 len,
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
    std::shared_ptr<UpdateUaSdk> upd = incomingQueue.popUpdate(&nReason);
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
    case ProcessReason::readComplete: {
        if (len && value) {
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
                UaVariant &data = upd->getData();
                UaString str;
                const void *s = nullptr;
                OpcUa_Int32 n = 0;
                if (data.type() == OpcUaType_ExtensionObject) {
                    UaExtensionObject extensionObject;
                    data.toExtensionObject(extensionObject);
                    UaStructureDefinition definition = pitem->structureDefinition(extensionObject.encodingTypeId());
                    if (definition.isUnion()) {
                        UaGenericUnionValue genericValue(extensionObject, definition);
                        int switchValue = genericValue.switchValue();
                        if (switchValue > 0) {
                            n = snprintf(value,
                                         len,
                                         "%s:%s",
                                         definition.child(switchValue - 1).name().toUtf8(),
                                         genericValue.value().toString().toUtf8());
                        } else {
                            (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                            ret = 1;
                        }
                    }
                } else {
                    const OpcUa_Variant *variant = data;
                    switch (variant->Datatype) {
                    case OpcUaType_Byte:
                    case OpcUaType_SByte:
                        if (variant->ArrayType == OpcUa_VariantArrayType_Array) {
                            n = variant->Value.Array.Length;
                            s = variant->Value.Array.Value.Array;
                        } else if (variant->ArrayType == OpcUa_VariantArrayType_Matrix) {
                            OpcUa_VariantMatrix_GetElementCount(&variant->Value.Matrix, &n);
                            s = variant->Value.Matrix.Value.Array;
                        }
                        if (static_cast<epicsUInt32>(n) > len)
                            n = len;
                        break;
                    case OpcUaType_ByteString:
                        n = printByteString(variant->Value.ByteString, value, len);
                    }
                    if (enumChoices) {
                        OpcUa_Int32 enumIndex;
                        data.toInt32(enumIndex);
                        const auto &it = enumChoices->find(enumIndex);
                        if (it != enumChoices->end() && !it->second.empty()) {
                            s = it->second.c_str();
                            n = static_cast<epicsUInt32>(it->second.length());
                            if (static_cast<epicsUInt32>(n) >= len)
                                n = len - 1;
                        }
                    }
                    if (!n) {
                        str = data.toString();
                        s = str.toUtf8();
                        n = str.length();
                        if (static_cast<epicsUInt32>(n) >= len)
                            n = len - 1;
                    }
                    if (s)
                        memcpy(value, s, n);
                }
                memset(value + n, 0, len - n);
                prec->udf = false;
                if (lenRead)
                    *lenRead = n;
            }
            if (statusCode)
                *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UaStatus(stat).toString().toUtf8(), statusTextLen);
                statusText[statusTextLen - 1] = '\0';
            }
        }
        break;
    }
    default:
        break;
    }

    prec->time = upd->getTimeStamp();
    if (nextReason)
        *nextReason = nReason;
    return ret;
}

void
DataElementUaSdkLeaf::dbgReadArray(const UpdateUaSdk *upd,
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
                      << UaStatus(upd->getStatus()).toString().toUtf8() << ") ";
            UaVariant &data = upd->getData();
            std::cout << " array of " << variantTypeString(data.type()) << "[" << upd->getData().arraySize() << "]"
                      << " into " << targetTypeName << "[" << targetSize << "]";
        } else {
            std::cout << "(client time " << time_buf << ") " << processReasonString(reason);
        }
        std::cout << " --- remaining queue " << incomingQueue.size() << "/" << incomingQueue.capacity() << std::endl;
    }
}

// Read array for EPICS String / OpcUa_String
long int
DataElementUaSdkLeaf::readArray(char *value,
                                epicsUInt32 len,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                OpcUa_BuiltInType expectedType,
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
    std::shared_ptr<UpdateUaSdk> upd = incomingQueue.popUpdate(&nReason);
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
    case ProcessReason::readComplete: {
        if (num && value) {
            OpcUa_StatusCode stat = upd->getStatus();
            if (OpcUa_IsNotGood(stat)) {
                // No valid OPC UA value
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                // Valid OPC UA value, so try to convert
                UaVariant &variant = upd->getData();
                if (!variant.isArray()) {
                    errlogPrintf("%s : incoming data is not an array\n", prec->name);
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    if (OpcUa_IsUncertain(stat)) {
                        (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                    }
                    elemsWritten = variant.arraySize();
                    if (elemsWritten > num)
                        elemsWritten = num;
                    const OpcUa_VariantArrayUnion &arrayData
                        = static_cast<const OpcUa_Variant *>(variant)->Value.Array.Value;
                    size_t l;
                    switch (variant.type()) {
                    case OpcUaType_String: {
                        for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                            l = OpcUa_String_StrSize(&arrayData.StringArray[i]);
                            if (l >= len)
                                l = len - 1;
                            memcpy(value + i * len, OpcUa_String_GetRawString(&arrayData.StringArray[i]), l);
                        }
                        break;
                    }
                    case OpcUaType_XmlElement: {
                        for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                            l = arrayData.XmlElementArray[i].Length;
                            if (l >= len)
                                l = len - 1;
                            memcpy(value + i * len, arrayData.XmlElementArray[i].Data, l);
                        }
                        break;
                    }
                    case OpcUaType_LocalizedText: {
                        for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                            l = OpcUa_String_StrSize(&arrayData.LocalizedTextArray[i].Text);
                            if (l >= len)
                                l = len - 1;
                            memcpy(
                                value + i * len, OpcUa_String_GetRawString(&arrayData.LocalizedTextArray[i].Text), l);
                        }
                        break;
                    }
                    case OpcUaType_QualifiedName: {
                        for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                            l = OpcUa_String_StrSize(&arrayData.QualifiedNameArray[i].Name);
                            if (l >= len)
                                l = len - 1;
                            memcpy(
                                value + i * len, OpcUa_String_GetRawString(&arrayData.QualifiedNameArray[i].Name), l);
                        }
                        break;
                    }
                    case OpcUaType_ByteString: {
                        for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                            printByteString(arrayData.ByteStringArray[i], value + i * len, len);
                        }
                        break;
                    }
                    default:
                        errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                     prec->name,
                                     variantTypeString(variant.type()),
                                     epicsTypeString(value));
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                        ret = 1;
                    }
                    prec->udf = false;
                }
            }
            if (statusCode)
                *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UaStatus(stat).toString().toUtf8(), statusTextLen);
                statusText[statusTextLen - 1] = '\0';
            }
        }
        break;
    }
    default:
        break;
    }

    prec->time = upd->getTimeStamp();
    if (nextReason)
        *nextReason = nReason;
    if (num && value)
        *numRead = elemsWritten;
    return ret;
}

// Specialization for epicsUInt8 / OpcUa_Byte
//   needed because UaByteArray API is different from all other UaXxxArray classes
//   and because epicsUInt8 is also used for arrays of OpcUa_Boolean
// CAVEAT: changes in the template (in DataElementUaSdk.h) must be reflected here
template<>
long
DataElementUaSdkLeaf::readArray(epicsUInt8 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                OpcUa_BuiltInType expectedType,
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
    case ProcessReason::readComplete: {
        if (num && value) {
            OpcUa_StatusCode stat = upd->getStatus();
            if (OpcUa_IsNotGood(stat)) {
                // No valid OPC UA value
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                // Valid OPC UA value, so try to convert
                UaVariant &variant = upd->getData();
                if (!variant.isArray() && variant.type() == OpcUaType_ByteString) {
                    const OpcUa_ByteString &bs = static_cast<const OpcUa_Variant *>(variant)->Value.ByteString;
                    elemsWritten = bs.Length;
                    if (elemsWritten > num)
                        elemsWritten = num;
                    memcpy(value, bs.Data, elemsWritten);
                } else if (!variant.isArray()) {
                    errlogPrintf("%s : incoming data is not an array\n", prec->name);
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else if (variant.type() != expectedType && variant.type() != OpcUaType_Boolean) {
                    errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                 prec->name,
                                 variantTypeString(variant.type()),
                                 epicsTypeString(*value));
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    if (OpcUa_IsUncertain(stat)) {
                        (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                    }
                    elemsWritten = variant.arraySize();
                    if (elemsWritten > num)
                        elemsWritten = num;
                    memcpy(value, static_cast<const OpcUa_Variant *>(variant)->Value.Array.Value.Array, elemsWritten);
                }
            }
            if (statusCode)
                *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UaStatus(stat).toString().toUtf8(), statusTextLen);
                statusText[statusTextLen - 1] = '\0';
            }
        }
        break;
    }
    default:
        break;
    }

    prec->time = upd->getTimeStamp();
    if (nextReason)
        *nextReason = nReason;
    if (num)
        *numRead = elemsWritten;
    return ret;
}

long
DataElementUaSdkLeaf::readArray(epicsInt8 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt8>(
        value, num, numRead, OpcUaType_SByte, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(epicsUInt8 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt8>(
        value, num, numRead, OpcUaType_Byte, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(epicsInt16 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt16>(
        value, num, numRead, OpcUaType_Int16, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(epicsUInt16 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt16>(
        value, num, numRead, OpcUaType_UInt16, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(epicsInt32 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt32>(
        value, num, numRead, OpcUaType_Int32, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(epicsUInt32 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt32>(
        value, num, numRead, OpcUaType_UInt32, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(epicsInt64 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt64>(
        value, num, numRead, OpcUaType_Int64, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(epicsUInt64 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt64>(
        value, num, numRead, OpcUaType_UInt64, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(epicsFloat32 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsFloat32>(
        value, num, numRead, OpcUaType_Float, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(epicsFloat64 *value,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray<epicsFloat64>(
        value, num, numRead, OpcUaType_Double, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementUaSdkLeaf::readArray(char *value,
                                epicsUInt32 len,
                                const epicsUInt32 num,
                                epicsUInt32 *numRead,
                                dbCommon *prec,
                                ProcessReason *nextReason,
                                epicsUInt32 *statusCode,
                                char *statusText,
                                const epicsUInt32 statusTextLen)
{
    return readArray(
        value, len, num, numRead, OpcUaType_String, prec, nextReason, statusCode, statusText, statusTextLen);
}

inline void
DataElementUaSdkLeaf::dbgWriteScalar() const
{
    if (isLeaf() && debug()) {
        std::cout << pconnector->getRecordName() << ": set outgoing data (" << variantTypeString(outgoingData.type())
        << ") to value ";
        if (outgoingData.type() == OpcUaType_String)
            std::cout << "'" << outgoingData.toString().toUtf8() << "'";
        else
            std::cout << outgoingData.toString().toUtf8();
        std::cout << std::endl;
    }
}

long
DataElementUaSdkLeaf::writeScalar(const epicsInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsInt32>(value, prec);
}

long
DataElementUaSdkLeaf::writeScalar(const epicsUInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsUInt32>(value, prec);
}

long
DataElementUaSdkLeaf::writeScalar(const epicsInt64 &value, dbCommon *prec)
{
    return writeScalar<epicsInt64>(value, prec);
}

long
DataElementUaSdkLeaf::writeScalar(const epicsFloat64 &value, dbCommon *prec)
{
    return writeScalar<epicsFloat64>(value, prec);
}

long
DataElementUaSdkLeaf::writeScalar(const char *value, epicsUInt32 len, dbCommon *prec)
{
    long ret = 1;
    long l;
    unsigned long ul;
    double d;
    char *end = nullptr;
    OpcUa_BuiltInType type = incomingData.type();

    if (type == OpcUaType_ExtensionObject) {
        UaExtensionObject extensionObject;
        incomingData.toExtensionObject(extensionObject);
        UaStructureDefinition definition = pitem->structureDefinition(extensionObject.encodingTypeId());
        if (definition.isUnion()) {
            if (value[0] == '\0') {
            } else
                for (int i = 0; i < definition.childrenCount(); i++) {
                    UaGenericUnionValue genericValue(extensionObject, definition);
                    const UaString &memberName = definition.child(i).name();
                    epicsUInt32 namelen = static_cast<epicsUInt32>(memberName.length());
                    if ((strncmp(value, memberName.toUtf8(), namelen) == 0) && value[namelen] == ':') {
                        // temporarily set incomingData to selected union member type
                        UaVariant saveValue = incomingData;
                        OpcUa_Variant fakeValue;
                        OpcUa_Variant_Initialize(&fakeValue);
                        fakeValue.Datatype = definition.child(i).valueType();
                        incomingData = fakeValue;
                        const UaNodeId &typeId = definition.child(i).typeId();
                        enumChoices = pitem->session->getEnumChoices(&typeId);
                        // recurse to set union member
                        ret = writeScalar(value + namelen + 1, len - (namelen + 1), prec);
                        // restore incomingData type to union
                        delete enumChoices;
                        enumChoices = nullptr;
                        incomingData = saveValue;
                        if (ret == 0) {
                            Guard G(outgoingLock);
                            genericValue.setValue(i + 1, outgoingData);
                            genericValue.toExtensionObject(extensionObject);
                            outgoingData.setExtensionObject(extensionObject, true);
                        }
                        return ret;
                    }
                }
        }
    }

    switch (type) {
    case OpcUaType_String: { // Scope of Guard G
        Guard G(outgoingLock);
        outgoingData.setString(UaByteString(len, (OpcUa_Byte *) value)); // UaByteString supports len
        markAsDirty();
        ret = 0;
        break;
    }
    case OpcUaType_XmlElement: { // Scope of Guard G
        Guard G(outgoingLock);
        outgoingData.setXmlElement(UaByteString(len, (OpcUa_Byte *) value));
        markAsDirty();
        ret = 0;
        break;
    }
    case OpcUaType_LocalizedText: { // Scope of Guard G
        Guard G(outgoingLock);
        UaLocalizedText localizedText;
        const char *sep = static_cast<const char *>(memchr(value, '|', len));
        if (sep) {
            localizedText.setLocale(UaByteString(static_cast<OpcUa_Int32>(sep - value), (OpcUa_Byte *) value));
            value = sep + 1;
            len -= static_cast<epicsUInt32>(sep + 1 - value);
        } else { // keep the locale
            incomingData.toLocalizedText(localizedText);
        }
        localizedText.setText(UaByteString(len, (OpcUa_Byte *) value));
        outgoingData.setLocalizedText(localizedText);
        markAsDirty();
        ret = 0;
        break;
    }
    case OpcUaType_QualifiedName: { // Scope of Guard G
        Guard G(outgoingLock);
        UaQualifiedName qualifiedName;
        const char *sep = static_cast<const char *>(memchr(value, '|', len));
        if (sep) {
            qualifiedName.setNamespaceIndex(atoi(value));
            value = sep + 1;
            len -= static_cast<epicsUInt32>(sep + 1 - value);
        } else { // keep the namespace
            incomingData.toQualifiedName(qualifiedName);
        }
        qualifiedName.setQualifiedName(UaByteString(len, (OpcUa_Byte *) value), qualifiedName.namespaceIndex());
        outgoingData.setQualifiedName(qualifiedName);
        markAsDirty();
        ret = 0;
        break;
    }
    case OpcUaType_ByteString: { // Scope of Guard G
        OpcUa_ByteString bs;
        OpcUa_ByteString_Initialize(&bs);
        if (parseByteString(bs, value, len) >= 0) {
            UaByteString byteString;
            byteString.attach(&bs); // wrap, no copy
            Guard G(outgoingLock);
            outgoingData.setByteString(byteString, true); // move, no copy
            markAsDirty();
            ret = 0;
        }
        break;
    }
    case OpcUaType_Boolean: { // Scope of Guard G
        Guard G(outgoingLock);
        if (strchr("YyTt1", *value))
            outgoingData.setBoolean(true);
        else
            outgoingData.setBoolean(false);
        markAsDirty();
        ret = 0;
        break;
    }
    case OpcUaType_Byte:
        ul = strtoul(value, &end, 0);
        if (end != value && isWithinRange<OpcUa_Byte>(ul)) {
            Guard G(outgoingLock);
            outgoingData.setByte(static_cast<OpcUa_Byte>(ul));
            markAsDirty();
            ret = 0;
        }
        break;
    case OpcUaType_SByte:
        l = strtol(value, &end, 0);
        if (end != value && isWithinRange<OpcUa_SByte>(l)) {
            Guard G(outgoingLock);
            outgoingData.setSByte(static_cast<OpcUa_SByte>(l));
            markAsDirty();
            ret = 0;
        }
        break;
    case OpcUaType_UInt16:
        ul = strtoul(value, &end, 0);
        if (end != value && isWithinRange<OpcUa_UInt16>(ul)) {
            Guard G(outgoingLock);
            outgoingData.setUInt16(static_cast<OpcUa_UInt16>(ul));
            markAsDirty();
            ret = 0;
        }
        break;
    case OpcUaType_Int16:
        l = strtol(value, &end, 0);
        if (end != value && isWithinRange<OpcUa_Int16>(l)) {
            Guard G(outgoingLock);
            outgoingData.setInt16(static_cast<OpcUa_Int16>(l));
            markAsDirty();
            ret = 0;
        }
        break;
    case OpcUaType_UInt32:
        ul = strtoul(value, &end, 0);
        if (end != value && isWithinRange<OpcUa_UInt32>(ul)) {
            Guard G(outgoingLock);
            outgoingData.setUInt32(static_cast<OpcUa_UInt32>(ul));
            markAsDirty();
            ret = 0;
        }
        break;
    case OpcUaType_Int32:
        l = strtol(value, &end, 0);
        if (enumChoices) {
            // first test enum strings then numeric values
            // in case a string starts with a number but means a different value
            for (const auto &it : *enumChoices)
                if (it.second == value) {
                    l = static_cast<long>(it.first);
                    ret = 0;
                    end = nullptr;
                    break;
                }
            if (ret != 0 && end != value)
                for (const auto &it : *enumChoices)
                    if (l == it.first) {
                        ret = 0;
                        break;
                    }
            if (ret != 0)
                break;
        }
        if (end != value && isWithinRange<OpcUa_Int32>(l)) {
            Guard G(outgoingLock);
            outgoingData.setInt32(static_cast<OpcUa_Int32>(l));
            markAsDirty();
            ret = 0;
        }
        break;
    case OpcUaType_UInt64:
        ul = strtoul(value, &end, 0);
        if (end != value && isWithinRange<OpcUa_UInt64>(ul)) {
            Guard G(outgoingLock);
            outgoingData.setUInt64(static_cast<OpcUa_UInt64>(ul));
            markAsDirty();
            ret = 0;
        }
        break;
    case OpcUaType_Int64:
        l = strtol(value, &end, 0);
        if (end != value && isWithinRange<OpcUa_Int64>(l)) {
            Guard G(outgoingLock);
            outgoingData.setInt64(static_cast<OpcUa_Int64>(l));
            markAsDirty();
            ret = 0;
        }
        break;
    case OpcUaType_Float:
        d = strtod(value, &end);
        if (end != value && isWithinRange<OpcUa_Float>(d)) {
            Guard G(outgoingLock);
            outgoingData.setFloat(static_cast<OpcUa_Float>(d));
            markAsDirty();
            ret = 0;
        }
        break;
    case OpcUaType_Double: {
        d = strtod(value, &end);
        if (end != value) {
            Guard G(outgoingLock);
            outgoingData.setDouble(static_cast<OpcUa_Double>(d));
            markAsDirty();
            ret = 0;
        }
        break;
    }
    default:
        errlogPrintf(
            "%s : unsupported conversion from string to %s for outgoing data\n", prec->name, variantTypeString(type));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    if (ret != 0) {
        errlogPrintf("%s : value \"%s\" out of range\n", prec->name, value);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
    }
    if (ret == 0)
        dbgWriteScalar();
    return ret;
}

inline void
DataElementUaSdkLeaf::dbgWriteArray(const epicsUInt32 targetSize, const std::string &targetTypeName) const
{
    if (isLeaf() && debug()) {
        std::cout << pconnector->getRecordName() << ": writing array of " << targetTypeName << "[" << targetSize
                  << "] as " << variantTypeString(outgoingData.type()) << "[" << outgoingData.arraySize() << "]"
                  << std::endl;
    }
}

// Write array for EPICS String / OpcUa_String
long
DataElementUaSdkLeaf::writeArray(
    const char *value, epicsUInt32 len, const epicsUInt32 num, OpcUa_BuiltInType targetType, dbCommon *prec)
{
    long ret = 0;

    if (!incomingData.isArray()) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
        switch (incomingData.type()) {
        case OpcUaType_String: {
            UaStringArray arr;
            arr.create(num);
            for (epicsUInt32 i = 0; i < num; i++) {
                UaString(UaByteString(len, (OpcUa_Byte *) value + i * len)).copyTo(&arr[i]);
            }
            { // Scope of Guard G
                Guard G(outgoingLock);
                UaVariant_set(outgoingData, arr);
                markAsDirty();
            }
            break;
        }
        case OpcUaType_XmlElement: {
            UaXmlElementArray arr;
            arr.create(num);
            for (epicsUInt32 i = 0; i < num; i++) {
                UaByteString(len, (OpcUa_Byte *) value + i * len).copyTo(&arr[i]);
            }
            { // Scope of Guard G
                Guard G(outgoingLock);
                UaVariant_set(outgoingData, arr);
                markAsDirty();
            }
            break;
        }
        case OpcUaType_LocalizedText: {
            UaLocalizedTextArray arr;
            OpcUa_UInt32 arraySize = incomingData.arraySize();
            const OpcUa_LocalizedText *incoming
                = static_cast<const OpcUa_Variant *>(incomingData)->Value.Array.Value.LocalizedTextArray;

            arr.create(num);
            for (epicsUInt32 i = 0; i < num; i++) {
                const char *sep = static_cast<const char *>(memchr(value, '|', len));
                if (sep) {
                    OpcUa_String_AttachToString(const_cast<char *>(value),
                                                static_cast<OpcUa_Int32>(sep - value),
                                                0,
                                                OpcUa_True,
                                                OpcUa_False,
                                                &arr[i].Locale);
                } else if (i < arraySize) {
                    OpcUa_String_StrnCpy(&arr[i].Locale, &incoming[i].Locale, OPCUA_STRING_LENDONTCARE);
                } else if (i > 0) {
                    OpcUa_String_StrnCpy(&arr[i].Locale, &arr[i - 1].Locale, OPCUA_STRING_LENDONTCARE);
                }
                OpcUa_String_AttachToString(const_cast<char *>(sep ? sep + 1 : value),
                                            OPCUA_STRINGLENZEROTERMINATED,
                                            0,
                                            OpcUa_True,
                                            OpcUa_False,
                                            &arr[i].Text);
                value += len;
            }
            { // Scope of Guard G
                Guard G(outgoingLock);
                UaVariant_set(outgoingData, arr);
                markAsDirty();
            }
            break;
        }
        case OpcUaType_QualifiedName: {
            UaQualifiedNameArray arr;
            OpcUa_UInt32 arraySize = incomingData.arraySize();
            const OpcUa_QualifiedName *incoming
                = static_cast<const OpcUa_Variant *>(incomingData)->Value.Array.Value.QualifiedNameArray;

            arr.create(num);
            for (epicsUInt32 i = 0; i < num; i++) {
                const char *sep = static_cast<const char *>(memchr(value, '|', len));
                if (sep) {
                    arr[i].NamespaceIndex = atoi(value);
                } else if (i < arraySize) {
                    arr[i].NamespaceIndex = incoming[i].NamespaceIndex;
                } else if (i > 0) {
                    arr[i].NamespaceIndex = arr[i - 1].NamespaceIndex;
                }
                OpcUa_String_AttachToString(const_cast<char *>(sep ? sep + 1 : value),
                                            OPCUA_STRINGLENZEROTERMINATED,
                                            0,
                                            OpcUa_True,
                                            OpcUa_False,
                                            &arr[i].Name);
                value += len;
            }
            { // Scope of Guard G
                Guard G(outgoingLock);
                UaVariant_set(outgoingData, arr);
                markAsDirty();
            }
            break;
        }
        default:
            errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                         prec->name,
                         variantTypeString(incomingData.type()),
                         variantTypeString(targetType),
                         epicsTypeString(value));
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        dbgWriteArray(num, epicsTypeString(value));
    }
    return ret;
}

// Specialization for epicsUInt8 / OpcUa_Byte
//   needed because UaByteArray API is different from all other UaXxxArray classes
//   and because epicsUInt8 is also used for arrays of OpcUa_Boolean
// CAVEAT: changes in the template (in DataElementUaSdk.h) must be reflected here
template<>
long
DataElementUaSdkLeaf::writeArray<epicsUInt8, UaByteArray, OpcUa_Byte>(const epicsUInt8 *value,
                                                                      const epicsUInt32 num,
                                                                      OpcUa_BuiltInType targetType,
                                                                      dbCommon *prec)
{
    long ret = 0;

    if (!incomingData.isArray() && incomingData.type() == OpcUaType_ByteString) {
        UaByteString bs(num, reinterpret_cast<OpcUa_Byte *>(const_cast<epicsUInt8 *>(value)));
        { // Scope of Guard G
            Guard G(outgoingLock);
            outgoingData.setByteString(bs, true); // move, no copy
            markAsDirty();
        }

        dbgWriteScalar();
    } else if (!incomingData.isArray()) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else if (incomingData.type() != OpcUaType_Byte && incomingData.type() != OpcUaType_Boolean) {
        errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                     prec->name,
                     variantTypeString(incomingData.type()),
                     variantTypeString(targetType),
                     epicsTypeString(*value));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
        if (incomingData.type() == OpcUaType_Byte) {
            UaByteArray arr(reinterpret_cast<const char *>(value), static_cast<OpcUa_Int32>(num));
            { // Scope of Guard G
                Guard G(outgoingLock);
                UaVariant_set(outgoingData, arr);
                markAsDirty();
            }
        } else {
            UaBooleanArray arr(static_cast<OpcUa_Int32>(num), const_cast<OpcUa_Boolean *>(value));
            { // Scope of Guard G
                Guard G(outgoingLock);
                UaVariant_set(outgoingData, arr);
                markAsDirty();
            }
        }
        dbgWriteArray(num, epicsTypeString(*value));
    }
    return ret;
}

long
DataElementUaSdkLeaf::writeArray(const epicsInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt8, UaSByteArray, OpcUa_SByte>(value, num, OpcUaType_SByte, prec);
}

long
DataElementUaSdkLeaf::writeArray(const epicsUInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt8, UaByteArray, OpcUa_Byte>(value, num, OpcUaType_Byte, prec);
}

long
DataElementUaSdkLeaf::writeArray(const epicsInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt16, UaInt16Array, OpcUa_Int16>(value, num, OpcUaType_Int16, prec);
}

long
DataElementUaSdkLeaf::writeArray(const epicsUInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt16, UaUInt16Array, OpcUa_UInt16>(value, num, OpcUaType_UInt16, prec);
}

long
DataElementUaSdkLeaf::writeArray(const epicsInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt32, UaInt32Array, OpcUa_Int32>(value, num, OpcUaType_Int32, prec);
}

long
DataElementUaSdkLeaf::writeArray(const epicsUInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt32, UaUInt32Array, OpcUa_UInt32>(value, num, OpcUaType_UInt32, prec);
}

long
DataElementUaSdkLeaf::writeArray(const epicsInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt64, UaInt64Array, OpcUa_Int64>(value, num, OpcUaType_Int64, prec);
}

long
DataElementUaSdkLeaf::writeArray(const epicsUInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt64, UaUInt64Array, OpcUa_UInt64>(value, num, OpcUaType_UInt64, prec);
}

long
DataElementUaSdkLeaf::writeArray(const epicsFloat32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat32, UaFloatArray, OpcUa_Float>(value, num, OpcUaType_Float, prec);
}

long
DataElementUaSdkLeaf::writeArray(const epicsFloat64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat64, UaDoubleArray, OpcUa_Double>(value, num, OpcUaType_Double, prec);
}

long
DataElementUaSdkLeaf::writeArray(const char *value, epicsUInt32 len, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray(value, len, num, OpcUaType_String, prec);
}

} // namespace DevOpcua
