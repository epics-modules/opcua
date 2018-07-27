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
#include <limits>
#include <string>

#include <uadatetime.h>
#include <opcua_builtintypes.h>

#include "DataElementUaSdk.h"
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
        default: return "Illegal Value";
    }
}

DataElementUaSdk::DataElementUaSdk (ItemUaSdk *item, const std::string &name)
    : DataElement(name)
    , pitem(item)
{}

void
DataElementUaSdk::setIncomingData (const UaDataValue &value)
{
    if (pconnector) {
        if (pconnector->debug() >= 5)
            std::cout << "Setting incoming data element and schedule processing"
                      << " for record " << pconnector->getRecordName() << std::endl;
        Guard(pconnector->lock);
        incomingData = value;
        incomingType = static_cast<OpcUa_BuiltInType>(value.value()->Datatype);
        pconnector->requestRecordProcessing(ProcessReason::incomingData);
    }
    //TODO: add structure support by calling DataElement children
}

epicsTimeStamp
DataElementUaSdk::readTimeStamp (bool server) const
{
    UaDateTime dt;
    OpcUa_UInt16 pico10;
    epicsTimeStamp ts;
    if (!server && incomingData.isSourceTimestampSet()) {
        dt = incomingData.sourceTimestamp();
        pico10 = incomingData.sourcePicoseconds();
    } else {
        dt = incomingData.serverTimestamp();
        pico10 = incomingData.serverPicoseconds();
    }
    ts.secPastEpoch = static_cast<epicsUInt32>(dt.toTime_t()) - POSIX_TIME_AT_EPICS_EPOCH;
    ts.nsec         = dt.msec() * 1000000L + pico10 / 100;

    if (pconnector->debug()) {
        char time_buf[40];
        epicsTimeToStrftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f", &ts);
        std::cout << pconnector->getRecordName() << ": reading "
                  << (server ? "server" : "device") << " timestamp ("
                  << time_buf << ")" << std::endl;
    }

    return ts;
}

epicsInt32
DataElementUaSdk::readInt32 () const
{
    OpcUa_Int32 v;
    UaVariant tempValue(*incomingData.value());

    if (tempValue.isEmpty())
        throw std::runtime_error(SB() << "no incoming data");

    if (pconnector->debug())
        std::cout << pconnector->getRecordName() << ": reading "
                  << tempValue.toString().toUtf8()
                  << " (" << variantTypeString(tempValue.type()) << ")"
                  << " as Int32" << std::endl;

    if (OpcUa_IsNotGood(tempValue.toInt32(v)))
        throw std::runtime_error(SB() << "incoming data out-of-bounds");
    return v;
}

epicsUInt32
DataElementUaSdk::readUInt32 () const
{
    OpcUa_UInt32 v;
    UaVariant tempValue(*incomingData.value());

    if (tempValue.isEmpty())
        throw std::runtime_error(SB() << "no incoming data");

    if (pconnector->debug())
        std::cout << pconnector->getRecordName() << ": reading "
                  << tempValue.toString().toUtf8()
                  << " (" << variantTypeString(tempValue.type()) << ")"
                  << " as UInt32" << std::endl;

    if (OpcUa_IsNotGood(tempValue.toUInt32(v)))
        throw std::runtime_error(SB() << "incoming data out-of-bounds");
    return v;
}

epicsFloat64
DataElementUaSdk::readFloat64 () const
{
    OpcUa_Double v;
    UaVariant tempValue(*incomingData.value());

    if (tempValue.isEmpty())
        throw std::runtime_error(SB() << "no incoming data");

    if (pconnector->debug())
        std::cout << pconnector->getRecordName() << ": reading "
                  << tempValue.toString().toUtf8()
                  << " (" << variantTypeString(tempValue.type()) << ")"
                  << " as Float64" << std::endl;

    if (OpcUa_IsNotGood(tempValue.toDouble(v)))
        throw std::runtime_error(SB() << "incoming data out-of-bounds");
    return v;
}

bool
DataElementUaSdk::readWasOk () const
{
    bool status = !!pitem->getReadStatus().isGood();
    if (pconnector->debug())
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
    if (pconnector->debug())
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

// Specialization to avoid compiler warnings
template<>
void checkRange<epicsInt32, OpcUa_UInt32> (const int &value) {
    if (value < 0)
        throw std::runtime_error(SB() << "outgoing data out-of-bounds");
}

void
DataElementUaSdk::writeInt32 (const epicsInt32 &value)
{
    UaVariant tempValue;

    switch (incomingType) {
    case OpcUaType_Boolean:
        if (value == 0)
            tempValue.setBoolean(false);
        else
            tempValue.setBoolean(true);
        break;
    case OpcUaType_Byte:
        checkRange<epicsInt32, OpcUa_Byte>(value);
        tempValue.setByte(static_cast<OpcUa_Byte>(value));
        break;
    case OpcUaType_SByte:
        checkRange<epicsInt32, OpcUa_SByte>(value);
        tempValue.setSByte(static_cast<OpcUa_SByte>(value));
        break;
    case OpcUaType_UInt16:
        checkRange<epicsInt32, OpcUa_UInt16>(value);
        tempValue.setUInt16(static_cast<OpcUa_UInt16>(value));
        break;
    case OpcUaType_Int16:
        checkRange<epicsInt32, OpcUa_Int16>(value);
        tempValue.setInt16(static_cast<OpcUa_Int16>(value));
        break;
    case OpcUaType_UInt32:
        checkRange<epicsInt32, OpcUa_UInt32>(value);
        tempValue.setUInt32(static_cast<OpcUa_UInt32>(value));
        break;
    case OpcUaType_Int32:
        tempValue.setInt32(value);
        break;
    case OpcUaType_UInt64:
        tempValue.setUInt64(static_cast<OpcUa_UInt64>(value));
        break;
    case OpcUaType_Int64:
        tempValue.setInt64(static_cast<OpcUa_Int64>(value));
        break;
    case OpcUaType_Float:
        tempValue.setFloat(static_cast<OpcUa_Float>(value));
        break;
    case OpcUaType_Double:
        tempValue.setDouble(static_cast<OpcUa_Double>(value));
        break;
    case OpcUaType_String:
        tempValue.setString(static_cast<UaString>(std::to_string(value).c_str()));
        break;
    default:
        throw std::runtime_error(SB() << "unsupported conversion for outgoing data");
    }

    if (pconnector->debug())
        std::cout << pconnector->getRecordName()
                  << ": set outgoing data ("
                  << variantTypeString(tempValue.type())
                  << ") to value " << tempValue.toString().toUtf8() << std::endl;

    outgoingData.setValue(tempValue, true); // true = detach variant from tempValue
}

void
DataElementUaSdk::requestRecordProcessing (const ProcessReason reason) const
{
    if (pconnector) {
        pconnector->requestRecordProcessing(reason);
    }
}

} // namespace DevOpcua
