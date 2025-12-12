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

#ifndef DEVOPCUA_DATAELEMENTOPEN62541LEAF_H
#define DEVOPCUA_DATAELEMENTOPEN62541LEAF_H

#include "DataElementOpen62541.h"
#include "RecordConnector.h"
#include "Update.h"
#include "UpdateQueue.h"

#include <errlog.h>
#include <recGbl.h>
#include <alarm.h>
#include <epicsTypes.h>

#include <string>
#include <limits>
#include <cstdlib>

namespace DevOpcua {

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
inline const char *epicsTypeString (const char*) { return "epicsString"; }

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

// Helper function to convert strings to numeric types

inline bool string_to(const std::string& s, epicsInt32& value) {
    try {
        value = std::stol(s, 0, 0);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool string_to(const std::string& s, epicsInt64& value) {
    try {
        value = std::stoll(s, 0, 0);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool string_to(const std::string& s, epicsUInt32& value) {
    try {
        long long v = std::stoll(s, 0, 0);
        if (!isWithinRange<epicsUInt32>(v)) return false;
        value = static_cast<epicsUInt32>(v);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool string_to(const std::string& s, epicsFloat64& value) {
    try {
        value = std::stod(s, 0);
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
class DataElementOpen62541Leaf
    : public DataElement
    , public DataElementOpen62541
{
public:
    /**
     * @brief Constructor for DataElement (leaf) from record connector.
     *
     * Creates the final (leaf) element of the data structure. The record connector
     * holds a shared pointer to its leaf, while the data element has a weak pointer
     * to the record connector.
     *
     * @param name        name of the element (empty for root element)
     * @param pitem       pointer to corresponding ItemOpen62541
     * @param pconnector  pointer to record connector to link to
     */
    DataElementOpen62541Leaf(const std::string &name, ItemOpen62541 *pitem, RecordConnector *pconnector);
    virtual ~DataElementOpen62541Leaf() override { delete enumChoices; }

    /* ElementTree node interface methods */
    virtual bool isLeaf () const override { return true; }
    virtual void addChild (std::weak_ptr<DataElementOpen62541> elem) override {}
    virtual std::shared_ptr<DataElementOpen62541> findChild (const std::string &name) const override
    {
        return std::shared_ptr<DataElementOpen62541>();
    }

    /**
     * @brief Create a DataElement and add it to the item's dataTree.
     *
     * @param item  item to add the element to
     * @param pconnector  pointer to the leaf's record connector
     * @param elementPath  full path to the element
     */
    static void
    addElementToTree(ItemOpen62541 *item, RecordConnector *pconnector, const std::list<std::string> &elementPath);

    virtual void show(const int level, const unsigned int indent) const override;


    virtual void
    setIncomingData(const UA_Variant &value, ProcessReason reason, const std::string *timefrom = nullptr) override;

    virtual void setIncomingEvent(ProcessReason reason) override;

    /**
     * @brief Set the EPICS-related state of the element and all sub-elements.
     */
    void setState(const ConnectionStatus state) override;

    /**
     * @brief Get the outgoing data value from the DataElement.
     *
     * Called from the OPC UA client worker thread when data is being
     * assembled in OPC UA session for sending.
     *
     * @return  reference to outgoing data
     */
    const UA_Variant &getOutgoingData() override;

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
     * DevOpcua::DataElement::readScalar(char*,const epicsUInt32,dbCommon*,ProcessReason*,epicsUInt32*,epicsUInt32*,char*,const epicsUInt32)
     */

    virtual long int readScalar(char *value, const epicsUInt32 len,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32* lenRead = nullptr,
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
    virtual long int readArray(char *value, epicsUInt32 len,
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
                                 const epicsUInt32 len,
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
    virtual bool isDirty () const override { return isdirty; }
    virtual void markAsDirty () override
    {
        isdirty = true;
        pitem->markAsDirty();
    }

    void dbgWriteScalar () const;
    void dbgReadScalar(const UpdateOpen62541 *upd,
                       const std::string &targetTypeName,
                       const size_t targetSize = 0) const;
    void dbgReadArray(const UpdateOpen62541 *upd,
                      const epicsUInt32 targetSize,
                      const std::string &targetTypeName) const;
    void checkWriteArray(const UA_DataType *expectedType, const std::string &targetTypeName) const;
    void dbgWriteArray(const epicsUInt32 targetSize, const std::string &targetTypeName) const;
    bool updateDataInStruct(void* container, std::shared_ptr<DataElementOpen62541Node> pelem);
    void createMap(const UA_DataType *type, const std::string* timefrom = nullptr);

    // Get the time stamp from the incoming object
    const epicsTime &getIncomingTimeStamp() const {
        ProcessReason reason = pitem->getReason();
        if ((reason == ProcessReason::incomingData || reason == ProcessReason::readComplete)
                && isLeaf())
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

    // Get the read status from the incoming object
    UA_StatusCode getIncomingReadStatus() const { return pitem->getLastStatus(); }

    // Read scalar value as templated function on EPICS type
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
        long ret = 1;

        if (incomingQueue.empty()) {
            errlogPrintf("%s: incoming data queue empty\n", prec->name);
            if (nextReason)
                *nextReason = ProcessReason::none;
            return 1;
        }

        ProcessReason nReason;
        std::shared_ptr<UpdateOpen62541> upd = incomingQueue.popUpdate(&nReason);
        dbgReadScalar(upd.get(), epicsTypeString(*value));
        prec->udf = false;

        switch (upd->getType()) {
        case ProcessReason::readFailure:
            (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            break;
        case ProcessReason::connectionLoss:
            (void) recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM);
            break;
        case ProcessReason::incomingData:
        case ProcessReason::readComplete:
        {
            if (value) {
                UA_StatusCode stat = upd->getStatus();
                if (UA_STATUS_IS_BAD(stat)) {
                    // No valid OPC UA value
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                } else {
                    // Valid OPC UA value, so try to convert
                    UA_Variant &data = upd->getData();
                    switch(typeKindOf(data)) {
                        case UA_DATATYPEKIND_BOOLEAN:
                            *value = (*static_cast<UA_Boolean*>(data.data) != 0);
                            ret = 0;
                            break;
                        case UA_DATATYPEKIND_BYTE:
                            if (isWithinRange<ET, UA_Byte>(*static_cast<UA_Byte*>(data.data))) {
                                *value = *static_cast<UA_Byte*>(data.data);
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_SBYTE:
                            if (isWithinRange<ET, UA_SByte>(*static_cast<UA_SByte*>(data.data))) {
                                *value = *static_cast<UA_SByte*>(data.data);
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_INT16:
                            if (isWithinRange<ET, UA_Int16>(*static_cast<UA_Int16*>(data.data))) {
                                *value = static_cast<ET>(*static_cast<UA_Int16*>(data.data));
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_UINT16:
                            if (isWithinRange<ET, UA_UInt16>(*static_cast<UA_UInt16*>(data.data))) {
                                *value = static_cast<ET>(*static_cast<UA_UInt16*>(data.data));
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_INT32:
                            if (isWithinRange<ET, UA_Int32>(*static_cast<UA_Int32*>(data.data))) {
                                *value = static_cast<ET>(*static_cast<UA_Int32*>(data.data));
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_UINT32:
                            if (isWithinRange<ET, UA_UInt32>(*static_cast<UA_UInt32*>(data.data))) {
                                *value = static_cast<ET>(*static_cast<UA_UInt32*>(data.data));
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_INT64:
                            if (isWithinRange<ET, UA_Int64>(*static_cast<UA_Int64*>(data.data))) {
                                *value = static_cast<ET>(*static_cast<UA_Int64*>(data.data));
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_UINT64:
                            if (isWithinRange<ET, UA_UInt64>(*static_cast<UA_UInt64*>(data.data))) {
                                *value = static_cast<ET>(*static_cast<UA_UInt64*>(data.data));
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_FLOAT:
                            if (isWithinRange<ET, UA_Float>(*static_cast<UA_Float*>(data.data))) {
                                *value = static_cast<ET>(*static_cast<UA_Float*>(data.data));
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_DOUBLE:
                            if (isWithinRange<ET, UA_Double>(*static_cast<UA_Double*>(data.data))) {
                                *value = static_cast<ET>(*static_cast<UA_Double*>(data.data));
                                ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_ENUM:
                            if (isWithinRange<ET, UA_Int32>(*static_cast<UA_Int32*>(data.data))) {
                                *value = static_cast<ET>(*static_cast<UA_Int32*>(data.data));
                                if (!enumChoices || enumChoices->find(static_cast<UA_UInt32>(*value)) != enumChoices->end())
                                    ret = 0;
                            }
                            break;
                        case UA_DATATYPEKIND_STRING:
                        {
                            UA_String* s = static_cast<UA_String*>(data.data);
                            if (string_to(std::string(reinterpret_cast<const char*>(s->data), s->length), *value))
                                ret = 0;
                            break;
                        }
                        case UA_DATATYPEKIND_LOCALIZEDTEXT:
                        {
                            UA_LocalizedText* lt = static_cast<UA_LocalizedText*>(data.data);
                            if (string_to(std::string(reinterpret_cast<const char*>(lt->text.data), lt->text.length), *value))
                                ret = 0;
                            break;
                        }
                        default:
                            errlogPrintf("%s : unsupported type kind %s for incoming data\n",
                                         prec->name, typeKindName(typeKindOf(data)));
                            (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    }
                    if (ret == 1) {
                        UA_String datastring = UA_STRING_NULL;
                        if (data.type)
                            UA_print(data.data, data.type, &datastring);
                        errlogPrintf("%s : incoming data (%s %.*s) out-of-bounds for %s\n",
                                     prec->name,
                                     variantTypeString(data),
                                     static_cast<int>(datastring.length), datastring.data,
                                     epicsTypeString(*value));
                        UA_String_clear(&datastring);
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    } else {
                        if (UA_STATUS_IS_UNCERTAIN(stat)) {
                            (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                        }
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
        return ret;
    }

    // Read array value as templated function on EPICS type
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
                    UA_Variant &variant = upd->getData();
                    if (UA_Variant_isScalar(&variant)) {
                        errlogPrintf("%s : incoming data is not an array\n", prec->name);
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                        ret = 1;
                    } else if (variant.type != expectedType) {
                        errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                     prec->name, variantTypeString(variant), epicsTypeString(*value));
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                        ret = 1;
                    } else {
                        if (UA_STATUS_IS_UNCERTAIN(stat)) {
                            (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                        }
                        elemsWritten = static_cast<epicsUInt32>(num) < variant.arrayLength ? num : static_cast<epicsUInt32>(variant.arrayLength);
                        memcpy(value, variant.data, sizeof(ET) * elemsWritten);
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

    // Read array value for EPICS String / UA_String
    long
    readArray (char *value, const epicsUInt32 len,
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
        long ret = 1;
        UA_StatusCode status = UA_STATUSCODE_BADUNEXPECTEDERROR;

        { // Scope of Guard G
            Guard G(outgoingLock);
            UA_Variant_clear(&outgoingData); // unlikely but we may still have unsent old data to discard
            switch (typeKindOf(incomingData)) {
            case UA_DATATYPEKIND_BOOLEAN:
            {
                UA_Boolean val = (value != 0);
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_BOOLEAN]);
                markAsDirty();
                ret = 0;
                break;
            }
            case UA_DATATYPEKIND_BYTE:
                if (isWithinRange<UA_Byte>(value)) {
                    UA_Byte val = static_cast<UA_Byte>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_BYTE]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_SBYTE:
                if (isWithinRange<UA_SByte>(value)) {
                    UA_SByte val = static_cast<UA_Byte>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_SBYTE]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_UINT16:
                if (isWithinRange<UA_UInt16>(value)) {
                    UA_UInt16 val = static_cast<UA_UInt16>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT16]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_INT16:
                if (isWithinRange<UA_Int16>(value)) {
                    UA_Int16 val = static_cast<UA_Int16>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT16]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_UINT32:
                if (isWithinRange<UA_UInt32>(value)) {
                    UA_UInt32 val = static_cast<UA_UInt32>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT32]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_ENUM:
            case UA_DATATYPEKIND_INT32:
                if (isWithinRange<UA_Int32>(value) &&
                    (!enumChoices ||
                        enumChoices->find(static_cast<UA_UInt32>(value)) != enumChoices->end())) {
                    UA_Int32 val = static_cast<UA_Int32>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT32]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_UINT64:
                if (isWithinRange<UA_UInt64>(value)) {
                    UA_UInt64 val = static_cast<UA_UInt64>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT64]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_INT64:
                if (isWithinRange<UA_Int64>(value)) {
                    UA_Int64 val = static_cast<UA_Int64>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT64]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_FLOAT:
                if (isWithinRange<UA_Float>(value)) {
                    UA_Float val = static_cast<UA_Float>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_FLOAT]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_DOUBLE:
                if (isWithinRange<UA_Double>(value)) {
                    UA_Double val = static_cast<UA_Double>(value);
                    status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_DOUBLE]);
                    markAsDirty();
                    ret = 0;
                }
                break;
            case UA_DATATYPEKIND_STRING:
            {
                std::string strval = std::to_string(value);
                UA_String val;
                val.length = strval.length();
                val.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(strval.c_str()));
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_STRING]);
                markAsDirty();
                ret = 0;
                break;
            }
            case UA_DATATYPEKIND_LOCALIZEDTEXT:
            {
                std::string strval = std::to_string(value);
                UA_LocalizedText val;
                val.locale = reinterpret_cast<const UA_LocalizedText*>(incomingData.data)->locale;
                val.text.length = strval.length();
                val.text.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(strval.c_str()));
                status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
                markAsDirty();
                ret = 0;
                break;
            }
            default:
                errlogPrintf("%s : unsupported conversion from %s to %s for outgoing data\n",
                             prec->name, epicsTypeString(value), variantTypeString(incomingData));
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            }
        }
        if (ret != 0) {
            errlogPrintf("%s : value out of range\n",
                         prec->name);
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

    // Write array value for EPICS String / UA_String
    long
    writeArray (const char *value, const epicsUInt32 len,
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
                UA_Variant_clear(&outgoingData); // unlikely but we may still have unsent old data to discard
                status = UA_Variant_setArrayCopy(&outgoingData, value, num, targetType);
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

    UpdateQueue<UpdateOpen62541> incomingQueue;  /**< queue of incoming values */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTOPEN62541LEAF_H
