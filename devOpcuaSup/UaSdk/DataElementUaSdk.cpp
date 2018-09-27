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
#include <cstdlib>

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

DataElementUaSdk::DataElementUaSdk (const std::string &name,
                                    ItemUaSdk *item,
                                    RecordConnector *pconnector)
    : DataElement(pconnector, name)
    , pitem(item)
{}

DataElementUaSdk::DataElementUaSdk (const std::string &name,
                                    ItemUaSdk *item,
                                    std::weak_ptr<DataElementUaSdk> child)
    : DataElement(name)
    , pitem(item)
{
    elements.push_back(child);
}

void
DataElementUaSdk::addElementChain (ItemUaSdk *item,
                                   RecordConnector *pconnector,
                                   const std::string &path)
{
    bool hasRootElement = true;
    // Create final path element as leaf and link it to connector
    std::string restpath;
    size_t sep = path.find_last_of(separator);
    std::string leafname = path.substr(sep + 1);
    if (sep != std::string::npos)
        restpath = path.substr(0, sep);

    auto chainelem = std::make_shared<DataElementUaSdk>(leafname, item, pconnector);
    pconnector->setDataElement(chainelem);

    // Starting from item...
    std::weak_ptr<DataElementUaSdk> topelem = item->rootElement;

    if (topelem.expired()) hasRootElement = false;

    // Simple case (leaf is the root element)
    if (leafname.empty()) {
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
        chainelem->parent = std::make_shared<DataElementUaSdk>("", item, chainelem);
        chainelem = chainelem->parent;
        item->rootElement = chainelem;
    }
}

void
DataElementUaSdk::setIncomingData (const UaDataValue &value)
{
    if (isLeaf() && debug >= 5) {
        std::cout << "Setting incoming data element"
                  << " for record " << pconnector->getRecordName() << std::endl;
        Guard(pconnector->lock);
        incomingData = value;
        incomingType = static_cast<OpcUa_BuiltInType>(value.value()->Datatype);
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
    ts.nsec         = static_cast<epicsUInt32>(dt.msec()) * 1000000 + pico10 / 100;

    if (isLeaf() && debug) {
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

    if (isLeaf() && debug) {
        std::cout << pconnector->getRecordName() << ": reading ";
        if (tempValue.type() == OpcUaType_String)
            std::cout << "'" << tempValue.toString().toUtf8() << "'";
        else
            std::cout << tempValue.toString().toUtf8();
        std::cout << " (" << variantTypeString(tempValue.type()) << ")"
                  << " as Int32" << std::endl;
    }

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

    if (isLeaf() && debug) {
        std::cout << pconnector->getRecordName() << ": reading ";
        if (tempValue.type() == OpcUaType_String)
            std::cout << "'" << tempValue.toString().toUtf8() << "'";
        else
            std::cout << tempValue.toString().toUtf8();
        std::cout << " (" << variantTypeString(tempValue.type()) << ")"
                  << " as UInt32" << std::endl;
    }

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

    if (isLeaf() && debug) {
        std::cout << pconnector->getRecordName() << ": reading ";
        if (tempValue.type() == OpcUaType_String)
            std::cout << "'" << tempValue.toString().toUtf8() << "'";
        else
            std::cout << tempValue.toString().toUtf8();
        std::cout << " (" << variantTypeString(tempValue.type()) << ")"
                  << " as Float64" << std::endl;
    }

    if (OpcUa_IsNotGood(tempValue.toDouble(v)))
        throw std::runtime_error(SB() << "incoming data out-of-bounds");
    return v;
}

void
DataElementUaSdk::readCString (char *value, const size_t num) const
{
    UaVariant tempValue(*incomingData.value());

    if (tempValue.isEmpty())
        throw std::runtime_error(SB() << "no incoming data");

    if (isLeaf() && debug) {
        std::cout << pconnector->getRecordName() << ": reading ";
        if (tempValue.type() == OpcUaType_String)
            std::cout << "'" << tempValue.toString().toUtf8() << "'";
        else
            std::cout << tempValue.toString().toUtf8();
        std::cout << " (" << variantTypeString(tempValue.type()) << ")"
                  << " as CString [" << num << "]" << std::endl;
    }

    if (num > 0) {
        strncpy(value, tempValue.toString().toUtf8(), num-1);
        value[num-1] = '\0';
    }
}

bool
DataElementUaSdk::readWasOk () const
{
    bool status = !!pitem->getReadStatus().isGood();

    if (isLeaf() && debug)
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
    if (isLeaf() && debug)
        std::cout << pconnector->getRecordName()
                  << ": write status is '"
                  << pitem->getWriteStatus().toString().toUtf8() << "'"
                  << std::endl;
    return status;
}

void DataElementUaSdk::clearIncomingData()
{
    incomingData.clear();
    if (isLeaf())
        pconnector->reason = ProcessReason::none;
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

void
DataElementUaSdk::printOutputDebugMessage (const RecordConnector *pconnector,
                                           const UaVariant &tempValue)
{
    if (pconnector) {
        std::cout << pconnector->getRecordName()
                  << ": set outgoing data ("
                  << variantTypeString(tempValue.type())
                  << ") to value ";
        if (tempValue.type() == OpcUaType_String)
            std::cout << "'" << tempValue.toString().toUtf8() << "'";
        else
            std::cout << tempValue.toString().toUtf8();
        std::cout << std::endl;
    }
}

void
DataElementUaSdk::writeInt32 (const epicsInt32 &value)
{
    UaVariant tempValue;

    switch (incomingType) {
    case OpcUaType_Int32:
        tempValue.setInt32(value);
        break;
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

    if (isLeaf() && debug)
        printOutputDebugMessage(pconnector, tempValue);

    outgoingData.setValue(tempValue, true); // true = detach variant from tempValue
}

void
DataElementUaSdk::writeUInt32 (const epicsUInt32 &value)
{
    UaVariant tempValue;

    switch (incomingType) {
    case OpcUaType_UInt32:
        tempValue.setUInt32(static_cast<OpcUa_UInt32>(value));
        break;
    case OpcUaType_Boolean:
        if (value == 0)
            tempValue.setBoolean(false);
        else
            tempValue.setBoolean(true);
        break;
    case OpcUaType_Byte:
        checkRange<epicsUInt32, OpcUa_Byte>(value);
        tempValue.setByte(static_cast<OpcUa_Byte>(value));
        break;
    case OpcUaType_SByte:
        checkRange<epicsUInt32, OpcUa_SByte>(value);
        tempValue.setSByte(static_cast<OpcUa_SByte>(value));
        break;
    case OpcUaType_UInt16:
        checkRange<epicsUInt32, OpcUa_UInt16>(value);
        tempValue.setUInt16(static_cast<OpcUa_UInt16>(value));
        break;
    case OpcUaType_Int16:
        checkRange<epicsUInt32, OpcUa_Int16>(value);
        tempValue.setInt16(static_cast<OpcUa_Int16>(value));
        break;
    case OpcUaType_Int32:
        checkRange<epicsUInt32, OpcUa_Int32>(value);
        tempValue.setInt32(static_cast<OpcUa_Int32>(value));
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

    if (isLeaf() && debug)
        printOutputDebugMessage(pconnector, tempValue);

    outgoingData.setValue(tempValue, true); // true = detach variant from tempValue
}

void
DataElementUaSdk::writeFloat64 (const epicsFloat64 &value)
{
    UaVariant tempValue;

    switch (incomingType) {
    case OpcUaType_Double:
        tempValue.setDouble(static_cast<OpcUa_Double>(value));
        break;
    case OpcUaType_Boolean:
        if (value == 0.)
            tempValue.setBoolean(false);
        else
            tempValue.setBoolean(true);
        break;
    case OpcUaType_Byte:
        checkRange<epicsFloat64, OpcUa_Byte>(value);
        tempValue.setByte(static_cast<OpcUa_Byte>(value));
        break;
    case OpcUaType_SByte:
        checkRange<epicsFloat64, OpcUa_SByte>(value);
        tempValue.setSByte(static_cast<OpcUa_SByte>(value));
        break;
    case OpcUaType_UInt16:
        checkRange<epicsFloat64, OpcUa_UInt16>(value);
        tempValue.setUInt16(static_cast<OpcUa_UInt16>(value));
        break;
    case OpcUaType_Int16:
        checkRange<epicsFloat64, OpcUa_Int16>(value);
        tempValue.setInt16(static_cast<OpcUa_Int16>(value));
        break;
    case OpcUaType_UInt32:
        checkRange<epicsFloat64, OpcUa_UInt32>(value);
        tempValue.setUInt32(static_cast<OpcUa_UInt32>(value));
        break;
    case OpcUaType_Int32:
        checkRange<epicsFloat64, OpcUa_Int32>(value);
        tempValue.setInt32(static_cast<OpcUa_Int32>(value));
        break;
    case OpcUaType_UInt64:
        checkRange<epicsFloat64, OpcUa_UInt64>(value);
        tempValue.setUInt64(static_cast<OpcUa_UInt64>(value));
        break;
    case OpcUaType_Int64:
        checkRange<epicsFloat64, OpcUa_Int64>(value);
        tempValue.setInt64(static_cast<OpcUa_Int64>(value));
        break;
    case OpcUaType_Float:
        checkRange<epicsFloat64, OpcUa_Float>(value);
        tempValue.setFloat(static_cast<OpcUa_Float>(value));
        break;
    case OpcUaType_String:
        tempValue.setString(static_cast<UaString>(std::to_string(value).c_str()));
        break;
    default:
        throw std::runtime_error(SB() << "unsupported conversion for outgoing data");
    }

    if (isLeaf() && debug)
        printOutputDebugMessage(pconnector, tempValue);

    outgoingData.setValue(tempValue, true); // true = detach variant from tempValue
}

void
DataElementUaSdk::writeCString(const char *value, const size_t num)
{
    long l;
    unsigned long ul;
    double d;
    UaVariant tempValue;

    switch (incomingType) {
    case OpcUaType_String:
        tempValue.setString(static_cast<UaString>(value));
        break;
    case OpcUaType_Boolean:
        if (strchr("YyTt1", *value))
            tempValue.setBoolean(true);
        else
            tempValue.setBoolean(false);
        break;
    case OpcUaType_Byte:
        ul = strtoul(value, nullptr, 0);
        checkRange<unsigned long, OpcUa_Byte>(ul);
        tempValue.setByte(static_cast<OpcUa_Byte>(ul));
        break;
    case OpcUaType_SByte:
        l = strtol(value, nullptr, 0);
        checkRange<long, OpcUa_SByte>(l);
        tempValue.setSByte(static_cast<OpcUa_SByte>(l));
        break;
    case OpcUaType_UInt16:
        ul = strtoul(value, nullptr, 0);
        checkRange<unsigned long, OpcUa_UInt16>(ul);
        tempValue.setUInt16(static_cast<OpcUa_UInt16>(ul));
        break;
    case OpcUaType_Int16:
        l = strtol(value, nullptr, 0);
        checkRange<long, OpcUa_Int16>(l);
        tempValue.setInt16(static_cast<OpcUa_Int16>(l));
        break;
    case OpcUaType_UInt32:
        ul = strtoul(value, nullptr, 0);
        checkRange<unsigned long, OpcUa_UInt32>(ul);
        tempValue.setUInt32(static_cast<OpcUa_UInt32>(ul));
        break;
    case OpcUaType_Int32:
        l = strtol(value, nullptr, 0);
        checkRange<long, OpcUa_Int32>(l);
        tempValue.setInt32(static_cast<OpcUa_Int32>(l));
        break;
    case OpcUaType_UInt64:
        ul = strtoul(value, nullptr, 0);
        checkRange<unsigned long, OpcUa_UInt64>(ul);
        tempValue.setUInt64(static_cast<OpcUa_UInt64>(ul));
        break;
    case OpcUaType_Int64:
        l = strtol(value, nullptr, 0);
        checkRange<long, OpcUa_Int64>(l);
        tempValue.setInt64(static_cast<OpcUa_Int64>(l));
        break;
    case OpcUaType_Float:
        d = strtod(value, nullptr);
        checkRange<double, OpcUa_Float>(d);
        tempValue.setFloat(static_cast<OpcUa_Float>(d));
        break;
    case OpcUaType_Double:
        d = strtod(value, nullptr);
        tempValue.setDouble(static_cast<OpcUa_Double>(d));
        break;
    default:
        throw std::runtime_error(SB() << "unsupported conversion for outgoing data");
    }

    if (isLeaf() && debug)
        printOutputDebugMessage(pconnector, tempValue);

    outgoingData.setValue(tempValue, true); // true = detach variant from tempValue
}

void
DataElementUaSdk::requestRecordProcessing (const ProcessReason reason) const
{
    if (isLeaf()) {
        pconnector->requestRecordProcessing(reason);
    }
}

} // namespace DevOpcua
