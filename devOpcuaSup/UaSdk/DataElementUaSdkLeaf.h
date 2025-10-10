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

#ifndef DEVOPCUA_DATAELEMENTUASDKLEAF_H
#define DEVOPCUA_DATAELEMENTUASDKLEAF_H

#include <limits>

#include "DataElementUaSdk.h"
#include "RecordConnector.h"
#include "Update.h"
#include "UpdateQueue.h"

namespace DevOpcua {

class ItemUaSdk;

typedef Update<UaVariant, OpcUa_StatusCode> UpdateUaSdk;

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
inline const char *epicsTypeString (const char *) { return "epicsString"; }

// Template for range check when writing
template<typename TO, typename FROM>
inline bool
isWithinRange (const FROM &value)
{
    return !(value < std::numeric_limits<TO>::lowest() || value > std::numeric_limits<TO>::max());
}

// Specializations for unsigned to signed to avoid compiler warnings
template<>
inline bool
isWithinRange<OpcUa_SByte, epicsUInt32>(const epicsUInt32 &value)
{
    return !(value > static_cast<OpcUa_UInt32>(std::numeric_limits<OpcUa_SByte>::max()));
}

template<>
inline bool
isWithinRange<OpcUa_Int16, epicsUInt32>(const epicsUInt32 &value)
{
    return !(value > static_cast<OpcUa_UInt32>(std::numeric_limits<OpcUa_Int16>::max()));
}

template<>
inline bool
isWithinRange<OpcUa_Int32, epicsUInt32>(const epicsUInt32 &value)
{
    return !(value > static_cast<OpcUa_UInt32>(std::numeric_limits<OpcUa_Int32>::max()));
}

template<>
inline bool
isWithinRange<OpcUa_SByte, epicsUInt64>(const epicsUInt64 &value)
{
    return !(value > static_cast<OpcUa_UInt64>(std::numeric_limits<OpcUa_SByte>::max()));
}

template<>
inline bool
isWithinRange<OpcUa_Int16, epicsUInt64>(const epicsUInt64 &value)
{
    return !(value > static_cast<OpcUa_UInt64>(std::numeric_limits<OpcUa_Int16>::max()));
}

template<>
inline bool
isWithinRange<OpcUa_Int32, epicsUInt64>(const epicsUInt64 &value)
{
    return !(value > static_cast<OpcUa_UInt64>(std::numeric_limits<OpcUa_Int32>::max()));
}

template<>
inline bool
isWithinRange<OpcUa_Int64, epicsUInt64>(const epicsUInt64 &value)
{
    return !(value > static_cast<OpcUa_UInt64>(std::numeric_limits<OpcUa_Int64>::max()));
}

// Simple-check specializations for converting signed to unsigned of same or wider type
template<>
inline bool
isWithinRange<OpcUa_UInt32, epicsInt8>(const epicsInt8 &value)
{
    return !(value < 0);
}
template<>
inline bool
isWithinRange<OpcUa_UInt32, epicsInt16>(const epicsInt16 &value)
{
    return !(value < 0);
}
template<>
inline bool
isWithinRange<OpcUa_UInt32, epicsInt32>(const epicsInt32 &value)
{
    return !(value < 0);
}
template<>
inline bool
isWithinRange<OpcUa_UInt64, epicsInt8>(const epicsInt8 &value)
{
    return !(value < 0);
}
template<>
inline bool
isWithinRange<OpcUa_UInt64, epicsInt16>(const epicsInt16 &value)
{
    return !(value < 0);
}
template<>
inline bool
isWithinRange<OpcUa_UInt64, epicsInt32>(const epicsInt32 &value)
{
    return !(value < 0);
}
template<>
inline bool
isWithinRange<OpcUa_UInt64, epicsInt64>(const epicsInt64 &value)
{
    return !(value < 0);
}

// No-check-needed specializations for converting to same or wider type
template<>
inline bool
isWithinRange<OpcUa_Int32, epicsInt32>(const epicsInt32 &)
{
    return true;
}
template<>
inline bool
isWithinRange<OpcUa_Int64, epicsInt32>(const epicsInt32 &)
{
    return true;
}
template<>
inline bool
isWithinRange<OpcUa_Float, epicsInt32>(const epicsInt32 &)
{
    return true;
}
template<>
inline bool
isWithinRange<OpcUa_Double, epicsInt32>(const epicsInt32 &)
{
    return true;
}

template<>
inline bool
isWithinRange<OpcUa_UInt32, epicsUInt32>(const epicsUInt32 &)
{
    return true;
}
template<>
inline bool
isWithinRange<OpcUa_Int64, epicsUInt32>(const epicsUInt32 &)
{
    return true;
}
template<>
inline bool
isWithinRange<OpcUa_UInt64, epicsUInt32>(const epicsUInt32 &)
{
    return true;
}
template<>
inline bool
isWithinRange<OpcUa_Float, epicsUInt32>(const epicsUInt32 &)
{
    return true;
}
template<>
inline bool
isWithinRange<OpcUa_Double, epicsUInt32>(const epicsUInt32 &)
{
    return true;
}

template<>
inline bool
isWithinRange<OpcUa_Int64, epicsInt64>(const epicsInt64 &)
{
    return true;
}
template<>
inline bool
isWithinRange<OpcUa_Float, epicsInt64>(const epicsInt64 &)
{
    return true;
}
template<>
inline bool
isWithinRange<OpcUa_Double, epicsInt64>(const epicsInt64 &)
{
    return true;
}

template<>
inline bool
isWithinRange<OpcUa_Double, epicsFloat64>(const epicsFloat64 &)
{
    return true;
}

class DataElementUaSdkLeaf
    : public DataElement
    , public DataElementUaSdk
{
public:
    /**
     * @brief Constructor for DataElement (leaf) from record connector.
     *
     * Creates a final leaf element of the data structure.
     * To avoid a circular dependency,
     * the record connector holds a shared pointer to its leaf,
     * while the data element has a weak pointer to the record connector.
     *
     * @param name        name of the element (empty for root element)
     * @param pitem       pointer to corresponding ItemUaSdk
     * @param pconnector  pointer to record connector to link to
     */
    DataElementUaSdkLeaf(const std::string &name, ItemUaSdk *pitem, RecordConnector *pconnector);
    virtual ~DataElementUaSdkLeaf () override { delete enumChoices; }

    /* ElementTree node interface methods */
    virtual bool isLeaf () const override { return true; }
    virtual void addChild (std::weak_ptr<DataElementUaSdk> elem) override {}
    virtual std::shared_ptr<DataElementUaSdk> findChild (const std::string &name) const override
    {
        return std::shared_ptr<DataElementUaSdk>();
    }

    /**
     * @brief Create a DataElement (leaf) and add it to the item's dataTree.
     *
     * @param item  item to add the element to
     * @param pconnector  pointer to the leaf's record connector
     * @param elementPath  full path to the element
     */
    static void
    addElementToTree(ItemUaSdk *item, RecordConnector *pconnector, const std::list<std::string> &elementPath);

    virtual void show(const int level, const unsigned int indent) const override;

    virtual void setIncomingData(const UaVariant &value,
                                 ProcessReason reason,
                                 const std::string *timefrom = nullptr,
                                 const UaNodeId *typeId = nullptr) override;
    virtual void setIncomingEvent(ProcessReason reason) override;
    virtual void setState(const ConnectionStatus state) override;
    virtual const UaVariant &getOutgoingData() override;
    virtual void clearOutgoingData() override;
    virtual void requestRecordProcessing(const ProcessReason reason) const override;
    virtual int debug() const override;

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
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

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
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

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
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

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
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as classic C string (char[]).
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readScalar(char*,const size_t,dbCommon*,ProcessReason*,epicsUInt32*,char*,const
     * epicsUInt32)
     */

    virtual long int readScalar(char *value,
                                const epicsUInt32 len,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *lenRead = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsInt8.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsInt8*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsInt8 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsUInt8.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsUInt8*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsUInt8 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsInt16.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsInt16*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsInt16 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsUInt16.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsUInt16*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsUInt16 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsInt32.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsInt32*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsInt32 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsUInt32.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsUInt32*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsUInt32 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsInt64.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsInt64*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsInt64 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsUInt64.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsUInt64*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsUInt64 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsFloat32.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsFloat32*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsFloat32 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of epicsFloat64.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(epicsFloat64*,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(epicsFloat64 *value,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Read incoming data as array of EPICS String (char[40]).
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::readArray(char*,const epicsUInt32,const
     * epicsUInt32,epicsUInt32*,dbCommon*,ProcessReason*,epicsUInt32*,char*,const epicsUInt32)
     */
    virtual long int readArray(char *value,
                               epicsUInt32 len,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1) override;

    /**
     * @brief Write outgoing scalar epicsInt32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const epicsInt32&,dbCommon*)
     */
    virtual long int writeScalar(const epicsInt32 &value, dbCommon *prec) override;

    /**
     * @brief Write outgoing scalar epicsInt64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const epicsInt64&,dbCommon*)
     */
    virtual long int writeScalar(const epicsInt64 &value, dbCommon *prec) override;

    /**
     * @brief Write outgoing scalar epicsUInt32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const epicsUInt32&,dbCommon*)
     */
    virtual long int writeScalar(const epicsUInt32 &value, dbCommon *prec) override;

    /**
     * @brief Write outgoing scalar epicsFloat64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const epicsFloat64&,dbCommon*)
     */
    virtual long int writeScalar(const epicsFloat64 &value, dbCommon *prec) override;

    /**
     * @brief Write outgoing classic C string (char[]) data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeScalar(const char*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeScalar(const char *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsInt8 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsInt8*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsInt8 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsUInt8 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsUInt8*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsUInt8 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsInt16 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsInt16*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsInt16 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsUInt16 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsUInt16*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsUInt16 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsInt32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsInt32*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsInt32 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsUInt32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsUInt32*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsUInt32 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsInt64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsInt64*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsInt64 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsUInt64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsUInt64*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsUInt64 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsFloat32 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsFloat32*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsFloat32 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of epicsFloat64 data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const epicsFloat64*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const epicsFloat64 *value, const epicsUInt32 num, dbCommon *prec) override;

    /**
     * @brief Write outgoing array of EPICS String (char[MAX_STRING_SIZE]) data.
     *
     * See the DataElement API method it overrides
     * DevOpcua::DataElement::writeArray(const char*,const epicsUInt32,dbCommon*)
     */
    virtual long int writeArray(const char *value, epicsUInt32 len, const epicsUInt32 num, dbCommon *prec) override;

private:
    virtual bool isDirty () const override { return isdirty; }
    virtual void markAsDirty () override
    {
        isdirty = true;
        pitem->markAsDirty();
    }

    void dbgWriteScalar() const;
    void dbgReadScalar(const UpdateUaSdk *upd, const std::string &targetTypeName, const size_t targetSize = 0) const;
    void dbgReadArray(const UpdateUaSdk *upd, const epicsUInt32 targetSize, const std::string &targetTypeName) const;
    void checkWriteArray(OpcUa_BuiltInType expectedType, const std::string &targetTypeName) const;
    void dbgWriteArray(const epicsUInt32 targetSize, const std::string &targetTypeName) const;

    const epicsTime &getIncomingTimeStamp() const;
    OpcUa_StatusCode getIncomingReadStatus() const;

    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, OpcUa_Int32 &value);
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, OpcUa_UInt32 &value);
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, OpcUa_Int64 &value);
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, OpcUa_Double &value);

    void UaVariant_set(UaVariant &variant, UaBooleanArray &value);
    void UaVariant_set(UaVariant &variant, UaSByteArray &value);
    void UaVariant_set(UaVariant &variant, UaByteArray &value);
    void UaVariant_set(UaVariant &variant, UaInt16Array &value);
    void UaVariant_set(UaVariant &variant, UaUInt16Array &value);
    void UaVariant_set(UaVariant &variant, UaInt32Array &value);
    void UaVariant_set(UaVariant &variant, UaUInt32Array &value);
    void UaVariant_set(UaVariant &variant, UaInt64Array &value);
    void UaVariant_set(UaVariant &variant, UaUInt64Array &value);
    void UaVariant_set(UaVariant &variant, UaFloatArray &value);
    void UaVariant_set(UaVariant &variant, UaDoubleArray &value);
    void UaVariant_set(UaVariant &variant, UaStringArray &value);
    void UaVariant_set(UaVariant &variant, UaXmlElementArray &value);
    void UaVariant_set(UaVariant &variant, UaLocalizedTextArray &value);
    void UaVariant_set(UaVariant &variant, UaQualifiedNameArray &value);

    // Read scalar value as templated function on EPICS type and OPC UA type
    // value == nullptr is allowed and leads to the value being dropped (ignored),
    // including the extended status
    template<typename ET, typename OT>
    long readScalar (ET *value,
                     dbCommon *prec,
                     ProcessReason *nextReason,
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
        case ProcessReason::readComplete: {
            if (value) {
                OpcUa_StatusCode stat = upd->getStatus();
                if (OpcUa_IsNotGood(stat)) {
                    // No valid OPC UA value
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    // Valid OPC UA value, so try to convert
                    OT v;
                    if (OpcUa_IsNotGood(UaVariant_to(upd->getData(), v))) {
                        errlogPrintf(
                            "%s : incoming data (%s) out-of-bounds\n", prec->name, upd->getData().toString().toUtf8());
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    } else {
                        if (OpcUa_IsUncertain(stat)) {
                            (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                        }
                        *value = v;
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
        return ret;
    }

    // Read array value as templated function on EPICS type and OPC UA type
    // (latter *must match* OPC UA type enum argument)
    // CAVEAT: changes must also be reflected in specializations (in DataElementUaSdk.cpp)
    template<typename ET>
    long readArray (ET *value,
                    const epicsUInt32 num,
                    epicsUInt32 *numRead,
                    OpcUa_BuiltInType expectedType,
                    dbCommon *prec,
                    ProcessReason *nextReason,
                    epicsUInt32 *statusCode,
                    char *statusText,
                    const epicsUInt32 statusTextLen = MAX_STRING_SIZE + 1)
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
                    if (!variant.isArray()) {
                        errlogPrintf("%s : incoming data is not an array\n", prec->name);
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                        ret = 1;
                    } else if (variant.type() != expectedType) {
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
                        memcpy(value,
                               static_cast<const OpcUa_Variant *>(variant)->Value.Array.Value.Array,
                               sizeof(ET) * elemsWritten);
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

    // Read array value for EPICS String / OpcUa_String
    long readArray(char *value,
                   epicsUInt32 len,
                   const epicsUInt32 num,
                   epicsUInt32 *numRead,
                   OpcUa_BuiltInType expectedType,
                   dbCommon *prec,
                   ProcessReason *nextReason,
                   epicsUInt32 *statusCode,
                   char *statusText,
                   const epicsUInt32 statusTextLen);

    // Write scalar value as templated function on EPICS type
    template<typename ET> long writeScalar (const ET &value, dbCommon *prec)
    {
        long ret = 1;

        switch (incomingData.type()) {
        case OpcUaType_Boolean: { // Scope of Guard G
            Guard G(outgoingLock);
            outgoingData.setBoolean(value != 0);
            markAsDirty();
            ret = 0;
            break;
        }
        case OpcUaType_Byte:
            if (isWithinRange<OpcUa_Byte>(value)) {
                Guard G(outgoingLock);
                outgoingData.setByte(static_cast<OpcUa_Byte>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_SByte:
            if (isWithinRange<OpcUa_SByte>(value)) {
                Guard G(outgoingLock);
                outgoingData.setSByte(static_cast<OpcUa_SByte>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_UInt16:
            if (isWithinRange<OpcUa_UInt16>(value)) {
                Guard G(outgoingLock);
                outgoingData.setUInt16(static_cast<OpcUa_UInt16>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_Int16:
            if (isWithinRange<OpcUa_Int16>(value)) {
                Guard G(outgoingLock);
                outgoingData.setInt16(static_cast<OpcUa_Int16>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_UInt32:
            if (isWithinRange<OpcUa_UInt32>(value)) {
                Guard G(outgoingLock);
                outgoingData.setUInt32(static_cast<OpcUa_UInt32>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_Int32: // may be an enum
            if (isWithinRange<OpcUa_Int32>(value)
                && (!enumChoices || enumChoices->find(static_cast<OpcUa_Int32>(value)) != enumChoices->end())) {
                Guard G(outgoingLock);
                outgoingData.setInt32(static_cast<OpcUa_Int32>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_UInt64:
            if (isWithinRange<OpcUa_UInt64>(value)) {
                Guard G(outgoingLock);
                outgoingData.setUInt64(static_cast<OpcUa_UInt64>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_Int64:
            if (isWithinRange<OpcUa_Int64>(value)) {
                Guard G(outgoingLock);
                outgoingData.setInt64(static_cast<OpcUa_Int64>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_Float:
            if (isWithinRange<OpcUa_Float>(value)) {
                Guard G(outgoingLock);
                outgoingData.setFloat(static_cast<OpcUa_Float>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_Double:
            if (isWithinRange<OpcUa_Double>(value)) {
                Guard G(outgoingLock);
                outgoingData.setDouble(static_cast<OpcUa_Double>(value));
                markAsDirty();
                ret = 0;
            }
            break;
        case OpcUaType_String: { // Scope of Guard G
            Guard G(outgoingLock);
            outgoingData.setString(static_cast<UaString>(std::to_string(value).c_str()));
            markAsDirty();
            break;
        }
        default:
            errlogPrintf("%s : unsupported conversion from %s to %s for outgoing data\n",
                         prec->name,
                         epicsTypeString(value),
                         variantTypeString(incomingData.type()));
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        }
        if (ret != 0) {
            errlogPrintf("%s : value out of range\n", prec->name);
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        }

        if (ret == 0)
            dbgWriteScalar();
        return ret;
    }

    // Write array value for EPICS String / OpcUa_String
    long
    writeArray(const char *value, epicsUInt32 len, const epicsUInt32 num, OpcUa_BuiltInType targetType, dbCommon *prec);

    // Write array value as templated function on EPICS type, OPC UA container and simple (element) types
    // (latter *must match* OPC UA type enum argument)
    // CAVEAT: changes must also be reflected in the specialization (in DataElementUaSdk.cpp)
    template<typename ET, typename CT, typename ST>
    long writeArray (const ET *value, const epicsUInt32 num, OpcUa_BuiltInType targetType, dbCommon *prec)
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
            // The array methods must cast away the constness of their value argument
            // as the UA SDK API uses non-const parameters
            ST *val = const_cast<ST *>(reinterpret_cast<const ST *>(value));
            CT arr(static_cast<OpcUa_Int32>(num), val);
            { // Scope of Guard G
                Guard G(outgoingLock);
                UaVariant_set(outgoingData, arr);
                markAsDirty();
            }

            dbgWriteArray(num, epicsTypeString(*value));
        }
        return ret;
    }

    int timesrc;
    UpdateQueue<UpdateUaSdk> incomingQueue; /**< queue of incoming values */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTUASDKLEAF_H
