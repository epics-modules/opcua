/*************************************************************************\
* Copyright (c) 2018-2020 ITER Organization.
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

#include <unordered_map>
#include <limits>

#include <open62541/client.h>
#define UA_STATUS_IS_BAD(status) (((status)&0x80000000)!=0)
#define UA_STATUS_IS_UNCERTAIN(status) (((status)&0x40000000)!=0)

#include <errlog.h>

#include "DataElement.h"
#include "devOpcua.h"
#include "RecordConnector.h"
#include "Update.h"
#include "UpdateQueue.h"
#include "ItemOpen62541.h"

namespace DevOpcua {

class ItemOpen62541;

typedef Update<UA_Variant, UA_StatusCode> UpdateOpen62541;

inline const char *epicsTypeString (const epicsInt8 &) { return "epicsInt8"; }
inline const char *epicsTypeString (const epicsUInt8 &) { return "epicsUInt8"; }
inline const char *epicsTypeString (const epicsInt16 &) { return "epicsInt16"; }
inline const char *epicsTypeString (const epicsUInt16 &) { return "epicsUInt16"; }
inline const char *epicsTypeString (const epicsInt32 &) { return "epicsInt32"; }
inline const char *epicsTypeString (const epicsUInt32 &) { return "epicsUInt32"; }
inline const char *epicsTypeString (const epicsInt64 &) { return "epicsInt64"; }
inline const char *epicsTypeString (const epicsUInt64 &) { return "epicsUInt64"; }
inline const char *epicsTypeString (const epicsFloat32 &) { return "epicsFloat32"; }
inline const char *epicsTypeString (const epicsFloat64 &) { return "epicsFloat64"; }
inline const char *epicsTypeString (const char* &) { return "epicsString"; }

inline const char *
variantTypeString (const UA_DataType *type)
{
#ifdef UA_ENABLE_TYPEDESCRIPTION
    if (type->typeName) return type->typeName;
#endif
    switch(type->typeKind) {
        case UA_TYPES_BOOLEAN:         return "UA_Boolean";
        case UA_TYPES_SBYTE:           return "UA_SByte";
        case UA_TYPES_BYTE:            return "UA_Byte";
        case UA_TYPES_INT16:           return "UA_Int16";
        case UA_TYPES_UINT16:          return "UA_UInt16";
        case UA_TYPES_INT32:           return "UA_Int32";
        case UA_TYPES_UINT32:          return "UA_UInt32";
        case UA_TYPES_INT64:           return "UA_Int64";
        case UA_TYPES_UINT64:          return "UA_UInt64";
        case UA_TYPES_FLOAT:           return "UA_Float";
        case UA_TYPES_DOUBLE:          return "UA_Double";
        case UA_TYPES_STRING:          return "UA_String";
        case UA_TYPES_DATETIME:        return "UA_DataTime";
        case UA_TYPES_GUID:            return "UA_Guid";
        case UA_TYPES_BYTESTRING:      return "UA_ByteString";
        case UA_TYPES_XMLELEMENT:      return "UA_XmlElement";
        case UA_TYPES_NODEID:          return "UA_NodeId";
        case UA_TYPES_EXPANDEDNODEID:  return "UA_ExpandedNodeId";
        case UA_TYPES_STATUSCODE:      return "UA_StatusCode";
        case UA_TYPES_QUALIFIEDNAME:   return "UA_QualifiedName";
        case UA_TYPES_LOCALIZEDTEXT:   return "UA_LocalizedText";
        case UA_TYPES_EXTENSIONOBJECT: return "UA_ExtensionObject";
        case UA_TYPES_DATAVALUE:       return "UA_DataValue";
        case UA_TYPES_VARIANT:         return "UA_Variant";
        case UA_TYPES_DIAGNOSTICINFO:  return "UA_DiagnosticInfo";
    }
    return "Illegal Value";
}

inline const char *
variantTypeString (const UA_Variant& v) {
    return variantTypeString(v.type);
}

// Template for range check when writing
template<typename TO, typename FROM>
inline bool isWithinRange (const FROM &value) {
    return !(value < std::numeric_limits<TO>::lowest() || value > std::numeric_limits<TO>::max());
}

// Specializations for unsigned to signed to avoid compiler warnings
template<>
inline bool isWithinRange<signed char, unsigned int> (const unsigned int &value) {
    return !(value > static_cast<unsigned int>(std::numeric_limits<signed char>::max()));
}

template<>
inline bool isWithinRange<short, unsigned int> (const unsigned int &value) {
    return !(value > static_cast<unsigned int>(std::numeric_limits<short>::max()));
}

template<>
inline bool isWithinRange<int, unsigned int> (const unsigned int &value) {
    return !(value > static_cast<unsigned int>(std::numeric_limits<int>::max()));
}

template<>
inline bool isWithinRange<int, unsigned long> (const unsigned long &value) {
    return !(value > static_cast<unsigned long>(std::numeric_limits<int>::max()));
}

template<>
inline bool isWithinRange<long, unsigned long> (const unsigned long &value) {
    return !(value > static_cast<unsigned long>(std::numeric_limits<long>::max()));
}

template<>
inline bool isWithinRange<long long, unsigned long> (const unsigned long &value) {
    return !(value > std::numeric_limits<long long>::max());
}

template<>
inline bool isWithinRange<signed char, unsigned long long> (const unsigned long long &value) {
    return !(value > static_cast<unsigned long long>(std::numeric_limits<signed char>::max()));
}

template<>
inline bool isWithinRange<short, unsigned long long> (const unsigned long long &value) {
    return !(value > static_cast<unsigned long long>(std::numeric_limits<short>::max()));
}

template<>
inline bool isWithinRange<int, unsigned long long> (const unsigned long long &value) {
    return !(value > static_cast<unsigned long long>(std::numeric_limits<int>::max()));
}

template<>
inline bool isWithinRange<long long, unsigned long long> (const unsigned long long &value) {
    return !(value > static_cast<unsigned long long>(std::numeric_limits<long long>::max()));
}

// Simple-check specializations for converting signed to unsigned of same or wider type
template<> inline bool isWithinRange<unsigned int, char> (const char &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned int, signed char> (const signed char &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned int, short> (const short &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned int, int> (const int &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long, char> (const char &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long, short> (const short &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long, int> (const int &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long, long> (const long &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long long, char> (const char &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long long, signed char> (const signed char &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long long, short> (const short &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long long, int> (const int &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long long, long> (const long &value) { return !(value < 0); }
template<> inline bool isWithinRange<unsigned long long, long long> (const long long &value) { return !(value < 0); }

// No-check-needed specializations for converting to same or wider type
template<> inline bool isWithinRange<int, int> (const int &) { return true; }
template<> inline bool isWithinRange<long, int> (const int &) { return true; }
template<> inline bool isWithinRange<long long, int> (const int &) { return true; }
template<> inline bool isWithinRange<float, int> (const int &) { return true; }
template<> inline bool isWithinRange<double, int> (const int &) { return true; }

template<> inline bool isWithinRange<unsigned int, unsigned int> (const unsigned int &) { return true; }
template<> inline bool isWithinRange<unsigned long, unsigned int> (const unsigned int &) { return true; }
template<> inline bool isWithinRange<long long, unsigned int> (const unsigned int &) { return true; }
template<> inline bool isWithinRange<unsigned long long, unsigned int> (const unsigned int &) { return true; }
template<> inline bool isWithinRange<float, unsigned int> (const unsigned int &) { return true; }
template<> inline bool isWithinRange<double, unsigned int> (const unsigned int &) { return true; }

template<> inline bool isWithinRange<long, long> (const long &) { return true; }
template<> inline bool isWithinRange<long long, long> (const long &) { return true; }

template<> inline bool isWithinRange<long long, long long> (const long long &) { return true; }
template<> inline bool isWithinRange<float, long long> (const long long &) { return true; }
template<> inline bool isWithinRange<double, long long> (const long long &) { return true; }

template<> inline bool isWithinRange<double, double> (const double &) { return true; }

// long long may or may not be the same size as long
template<>
inline bool isWithinRange<unsigned long, long long> (const long long &value) {
    return sizeof(long long) > sizeof(long) ? !(value < 0 || value > static_cast<long long>(std::numeric_limits<unsigned long>::max())) : !(value < 0);
}

// Helper function to convert C-strings to numeric types

inline bool string_to(const char* s, epicsInt32& value) {
    try {
        value = std::stol(std::string(s), 0, 0);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool string_to(const char* s, epicsInt64& value) {
    try {
        value = std::stoll(std::string(s), 0, 0);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool string_to(const char* s, epicsUInt32& value) {
    try {
        long long v = std::stoll(std::string(s), 0, 0);
        if (!isWithinRange<epicsUInt32>(v)) return false;
        value = static_cast<epicsUInt32>(v);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool string_to(const char* s, epicsFloat64& value) {
    try {
        value = std::stod(std::string(s), 0);
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief The DataElementOpen62541 implementation of a single piece of data.
 *
 * See DevOpcua::DataElement
 */
class DataElementOpen62541 : public DataElement
{
public:
    /**
     * @brief Constructor for DataElement from record connector.
     *
     * Creates the final (leaf) element of the data structure. The record connector
     * holds a shared pointer to its leaf, while the data element has a weak pointer
     * to the record connector.
     *
     * @param name        name of the element (empty for root element)
     * @param pitem       pointer to corresponding ItemOpen62541
     * @param pconnector  pointer to record connector to link to
     */
    DataElementOpen62541(const std::string &name,
                     ItemOpen62541 *pitem,
                     RecordConnector *pconnector);

    /**
     * @brief Constructor for DataElement from child element.
     *
     * Creates an intermediate (node) element of the data structure. The child holds
     * a shared pointer, while the parent has a weak pointer in its list/map of child
     * nodes, to facilitate traversing the structure when data updates come in.
     *
     * @param name   name of the element
     * @param item   pointer to corresponding ItemOpen62541
     * @param child  weak pointer to child
     */
    DataElementOpen62541(const std::string &name,
                     ItemOpen62541 *item,
                     std::weak_ptr<DataElementOpen62541> child);

    /**
     * @brief Construct a linked list of data elements between a record connector and an item.
     *
     * Creates the leaf element first, then identifies the part of the path that already exists
     * on the item and creates the missing list of linked nodes.
     *
     * @param pitem       pointer to corresponding ItemOpen62541
     * @param pconnector  pointer to record connector to link to
     * @param path        path of leaf element inside the structure
     */
    static void addElementToTree(ItemOpen62541 *item,
                                 RecordConnector *pconnector,
                                 const std::string &path);

    /**
     * @brief Print configuration and status. See DevOpcua::DataElement::show
     */
    void show(const int level, const unsigned int indent) const override;

    /**
     * @brief Push an incoming data value into the DataElement.
     *
     * Called from the OPC UA client worker thread when new data is
     * received from the OPC UA session.
     *
     * @param value  new value for this data element
     * @param reason  reason for this value update
     */
    void setIncomingData(const UA_Variant &value, ProcessReason reason);

    /**
     * @brief Push an incoming event into the DataElement.
     *
     * Called from the OPC UA client worker thread when an out-of-band
     * event was received (connection loss).
     *
     * @param reason  reason for this value update
     */
    void setIncomingEvent(ProcessReason reason);

    /**
     * @brief Get the outgoing data value from the DataElement.
     *
     * Called from the OPC UA client worker thread when data is being
     * assembled in OPC UA session for sending.
     *
     * @return  reference to outgoing data
     */
    const UA_Variant &getOutgoingData();

    /**
     * @brief Read incoming data as a scalar epicsInt32.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readScalar(epicsInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readScalar(epicsInt32 *value,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as a scalar epicsInt64.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readScalar(epicsInt64*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readScalar(epicsInt64 *value,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as a scalar epicsUInt32.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readScalar(epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readScalar(epicsUInt32 *value,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as a scalar epicsFloat64.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readScalar(epicsFloat64*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readScalar(epicsFloat64 *value,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as classic C string (char[]).
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readScalar(char*,const size_t,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */

    virtual long int readScalar(char *value, const size_t num,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsInt8.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsInt8*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsInt8 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsUInt8.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsUInt8*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsUInt8 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsInt16.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsInt16*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsInt16 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsUInt16.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsUInt16*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsUInt16 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsInt32.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsInt32*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsInt32 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsUInt32.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsUInt32*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsUInt32 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsInt64.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsInt64*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsInt64 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsUInt64.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsUInt64*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsUInt64 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsFloat32.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsFloat32*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsFloat32 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of epicsFloat64.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsFloat64*,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsFloat64 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Read incoming data as array of EPICS String (char[40]).
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(char*,const epicsUInt32,const epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(char *value, const epicsUInt32 len,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    /**
     * @brief Write outgoing scalar epicsInt32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const epicsInt32&,dbCommon*)
     */
    virtual long int writeScalar(const epicsInt32 &value,
                                 dbCommon *prec) override;

    /**
     * @brief Write outgoing scalar epicsInt64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const epicsInt64&,dbCommon*)
     */
    virtual long int writeScalar(const epicsInt64 &value,
                                 dbCommon *prec) override;

    /**
     * @brief Write outgoing scalar epicsUInt32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const epicsUInt32&,dbCommon*)
     */
    virtual long int writeScalar(const epicsUInt32 &value,
                                 dbCommon *prec) override;

    /**
     * @brief Write outgoing scalar epicsFloat64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const epicsFloat64&,dbCommon*)
     */
    virtual long int writeScalar(const epicsFloat64 &value,
                                 dbCommon *prec) override;

    /**
     * @brief Write outgoing classic C string (char[]) data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const char*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeScalar(const char *value,
                                 const epicsUInt32 num,
                                 dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsInt8 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsInt8*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsInt8 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsUInt8 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsUInt8*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsUInt8 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsInt16 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsInt16*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsInt16 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsUInt16 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsUInt16*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsUInt16 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsInt32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsInt32*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsInt32 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsUInt32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsUInt32*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsUInt32 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsInt64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsInt64*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsInt64 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsUInt64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsUInt64*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsUInt64 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsFloat32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsFloat32*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsFloat32 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsFloat64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsFloat64*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsFloat64 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    /**
     * @brief Write outgoing array of EPICS String (char[MAX_STRING_SIZE]) data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const char*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const char *value, const epicsUInt32 len,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

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
    virtual void clearOutgoingData() { UA_Variant_clear(&outgoingData); }

    /**
     * @brief Create processing requests for record(s) attached to this element.
     * See DevOpcua::DataElement::requestRecordProcessing
     */
    virtual void requestRecordProcessing(const ProcessReason reason) const override;

    /**
     * @brief Get debug level from record (via RecordConnector).
     * @return debug level
     */
    int debug() const { return (isLeaf() ? pconnector->debug() : pitem->debug()); }

private:
    void dbgWriteScalar () const;
    void dbgReadScalar(const UpdateOpen62541 *upd,
                       const std::string &targetTypeName,
                       const size_t targetSize = 0) const;
    void dbgReadArray(const UpdateOpen62541 *upd,
                      const epicsUInt32 targetSize,
                      const std::string &targetTypeName) const;
    void checkWriteArray(const UA_DataType *expectedType, const std::string &targetTypeName) const;
    void dbgWriteArray(const epicsUInt32 targetSize, const std::string &targetTypeName) const;
/*
    bool updateDataInGenericValue(UaGenericStructureValue &value,
                                  const int index,
                                  std::shared_ptr<DataElementOpen62541> pelem);
*/
    // Structure always returns true to ensure full traversal
    bool isDirty() const { return isdirty || !isleaf; }

    // Get the time stamp from the incoming object
    const epicsTime &getIncomingTimeStamp() const {
        ProcessReason reason = pitem->getReason();
        if ((reason == ProcessReason::incomingData || reason == ProcessReason::readComplete)
                && isLeaf())
            if (pconnector->plinkinfo->useServerTimestamp)
                return pitem->tsServer;
            else
                return pitem->tsSource;
        else
            return pitem->tsClient;
    }

    // Get the read status from the incoming object
    UA_StatusCode getIncomingReadStatus() const { return pitem->getLastStatus(); }

    // Read scalar value as templated function on EPICS type and OPC UA type
    // value == nullptr is allowed and leads to the value being dropped (ignored),
    // including the extended status
    template<typename ET>
    long
    readScalar (ET *value,
                dbCommon *prec,
                ProcessReason *nextReason,
                epicsUInt32 *statusCode,
                char *statusText,
                const epicsUInt32 statusTextLen)
    {
        long ret = 0;


        if (incomingQueue.empty()) {
            errlogPrintf("%s : incoming data queue empty\n", prec->name);
            return 1;
        }

        ProcessReason nReason;
        std::shared_ptr<UpdateOpen62541> upd = incomingQueue.popUpdate(&nReason);
        dbgReadScalar(upd.get(), epicsTypeString(*value));

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
            if (value) {
                UA_StatusCode stat = upd->getStatus();
                if (UA_STATUS_IS_BAD(stat)) {
                    // No valid OPC UA value
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    // Valid OPC UA value, so try to convert
                    UA_Variant &data = upd->getData();
                    switch(data.type->typeKind) {
                        case UA_TYPES_BOOLEAN:
                            *value = *static_cast<UA_Boolean*>(data.data) != 0;
                            break;
                        case UA_TYPES_BYTE:
                            if (isWithinRange<ET, UA_Byte>(*static_cast<UA_Byte*>(data.data)))
                                *value = *static_cast<UA_Byte*>(data.data);
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_SBYTE:
                            if (isWithinRange<ET, UA_SByte>(*static_cast<UA_SByte*>(data.data)))
                                *value = *static_cast<UA_SByte*>(data.data);
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_INT16:
                            if (isWithinRange<ET, UA_Int16>(*static_cast<UA_Int16*>(data.data)))
                                *value = static_cast<ET>(*static_cast<UA_Int16*>(data.data));
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_UINT16:
                            if (isWithinRange<ET, UA_UInt16>(*static_cast<UA_UInt16*>(data.data)))
                                *value = static_cast<ET>(*static_cast<UA_UInt16*>(data.data));
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_INT32:
                            if (isWithinRange<ET, UA_Int32>(*static_cast<UA_Int32*>(data.data)))
                                *value = static_cast<ET>(*static_cast<UA_Int32*>(data.data));
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_UINT32:
                            if (isWithinRange<ET, UA_UInt32>(*static_cast<UA_UInt32*>(data.data)))
                                *value = static_cast<ET>(*static_cast<UA_UInt32*>(data.data));
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_INT64:
                            if (isWithinRange<ET, UA_Int64>(*static_cast<UA_Int64*>(data.data)))
                                *value = static_cast<ET>(*static_cast<UA_Int64*>(data.data));
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_UINT64:
                            if (isWithinRange<ET, UA_UInt64>(*static_cast<UA_UInt64*>(data.data)))
                                *value = static_cast<ET>(*static_cast<UA_UInt64*>(data.data));
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_FLOAT:
                            if (isWithinRange<ET, UA_Float>(*static_cast<UA_Float*>(data.data)))
                                *value = static_cast<ET>(*static_cast<UA_Float*>(data.data));
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_DOUBLE:
                            if (isWithinRange<ET, UA_Double>(*static_cast<UA_Double*>(data.data)))
                                *value = static_cast<ET>(*static_cast<UA_Double*>(data.data));
                            else
                                ret = 1;
                            break;
                        case UA_TYPES_STRING:
                            if (!string_to(reinterpret_cast<char*>(static_cast<UA_String*>(data.data)->data), *value))
                                ret = 1;
                            break;
                    }
                    if (ret == 1) {
                        UA_String datastring;
                        UA_print(&data, data.type, &datastring);
                        errlogPrintf("%s : incoming data (%s) out-of-bounds\n",
                                     prec->name,
                                     datastring.data);
                        UA_String_clear(&datastring);
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    } else {
                        if (UA_STATUS_IS_UNCERTAIN(stat)) {
                            (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                        }
                        prec->udf = false;
                    }
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

    // Read array value as templated function on EPICS type and OPC UA type
    // (latter *must match* OPC UA type enum argument)
    // CAVEAT: changes must also be reflected in specializations (in DataElementOpen62541.cpp)
    template<typename ET>
    long
    readArray (ET *value, const epicsUInt32 num,
               epicsUInt32 *numRead,
               const UA_DataType *expectedType,
               dbCommon *prec,
               ProcessReason *nextReason,
               epicsUInt32 *statusCode,
               char *statusText,
               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1)
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
                    UA_Variant &data = upd->getData();
                    if (UA_Variant_isScalar(&data)) {
                        errlogPrintf("%s : incoming data is not an array\n", prec->name);
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                        ret = 1;
                    } else if (data.type != expectedType) {
                        errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                     prec->name, variantTypeString(data), epicsTypeString(*value));
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                        ret = 1;
                    } else {
                        if (UA_STATUS_IS_UNCERTAIN(stat)) {
                            (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                        }
                        elemsWritten = static_cast<epicsUInt32>(num) < data.arrayLength ? num : static_cast<epicsUInt32>(data.arrayLength);
                        memcpy(value, data.data, sizeof(ET) * elemsWritten);
                        prec->udf = false;
                    }
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

    // Read array value for EPICS String / UA_String
    long
    readArray (char **value, const epicsUInt32 len,
               const epicsUInt32 num,
               epicsUInt32 *numRead,
               const UA_DataType *expectedType,
               dbCommon *prec,
               ProcessReason *nextReason,
               epicsUInt32 *statusCode,
               char *statusText,
               const epicsUInt32 statusTextLen);

    // Write scalar value as templated function on EPICS type
    template<typename ET>
    long
    writeScalar (const ET &value,
                 dbCommon *prec)
    {
        long ret = 0;
        UA_StatusCode status = UA_STATUSCODE_BADUNEXPECTEDERROR;

        switch (incomingData.type->typeKind) {
        case UA_TYPES_BOOLEAN:
        { // Scope of Guard G
            Guard G(outgoingLock);
            isdirty = true;
            UA_Boolean val = (value != 0);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_BOOLEAN]);
            break;
        }
        case UA_TYPES_BYTE:
            if (isWithinRange<UA_Byte>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_Byte val = static_cast<UA_Byte>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_BYTE]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_SBYTE:
            if (isWithinRange<UA_SByte>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_SByte val = static_cast<UA_Byte>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_SBYTE]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_UINT16:
            if (isWithinRange<UA_UInt16>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_UInt16 val = static_cast<UA_UInt16>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT16]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_INT16:
            if (isWithinRange<UA_Int16>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_Int16 val = static_cast<UA_Int16>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT16]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_UINT32:
            if (isWithinRange<UA_UInt32>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_UInt32 val = static_cast<UA_UInt32>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT16]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_INT32:
            if (isWithinRange<UA_Int32>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_Int32 val = static_cast<UA_Int32>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT16]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_UINT64:
            if (isWithinRange<UA_UInt64>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_UInt64 val = static_cast<UA_UInt64>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT64]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_INT64:
            if (isWithinRange<UA_Int64>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_Int64 val = static_cast<UA_Int64>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT64]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_FLOAT:
            if (isWithinRange<UA_Float>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_Float val = static_cast<UA_Float>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_FLOAT]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_DOUBLE:
            if (isWithinRange<UA_Double>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                UA_Double val = static_cast<UA_Double>(value);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_DOUBLE]);
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case UA_TYPES_STRING:
        {
            std::string strval = std::to_string(value);
            UA_String val;
            val.length = strval.length();
            val.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(strval.c_str()));
            { // Scope of Guard G
                Guard G(outgoingLock);
                isdirty = true;
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_STRING]);
            }
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

    // Write array value for EPICS String / UA_String
    long
    writeArray (const char **value, const epicsUInt32 len,
                const epicsUInt32 num,
                const UA_DataType *targetType,
                dbCommon *prec);

    // Write array value as templated function on EPICS type, OPC UA container and simple (element) types
    // (latter *must match* OPC UA type enum argument)
    // CAVEAT: changes must also be reflected in the specialization (in DataElementOpen62541.cpp)
    template<typename ET>
    long
    writeArray (const ET *value, const epicsUInt32 num,
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
            UA_StatusCode status;
            { // Scope of Guard G
                Guard G(outgoingLock);
                isdirty = true;
                status = UA_Variant_setArrayCopy(&outgoingData, value, num, targetType);
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

    ItemOpen62541 *pitem;                                       /**< corresponding item */
    std::vector<std::weak_ptr<DataElementOpen62541>> elements;  /**< children (if node) */
    std::shared_ptr<DataElementOpen62541> parent;               /**< parent */

    std::unordered_map<int, std::weak_ptr<DataElementOpen62541>> elementMap;

    bool mapped;                             /**< child name to index mapping done */
    UpdateQueue<UpdateOpen62541> incomingQueue;  /**< queue of incoming values */
    UA_Variant incomingData;                 /**< cache of latest incoming value */
    epicsMutex outgoingLock;                 /**< data lock for outgoing value */
    UA_Variant outgoingData;                 /**< cache of latest outgoing value */
    bool isdirty;                            /**< outgoing value has been (or needs to be) updated */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTOPEN62541_H
