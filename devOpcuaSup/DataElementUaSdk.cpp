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

#include <iostream>

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

DataElementUaSdk::DataElementUaSdk (const std::string &name)
    : DataElement(name)
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
        pconnector->requestRecordProcessing();
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
DataElementUaSdk::readInt32() const
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
DataElementUaSdk::readUInt32() const
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
DataElementUaSdk::readFloat64() const
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

} // namespace DevOpcua
