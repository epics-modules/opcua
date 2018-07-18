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

#ifndef DEVOPCUA_DATAELEMENTUASDK_H
#define DEVOPCUA_DATAELEMENTUASDK_H

#include <uadatavalue.h>

#include "DataElement.h"

namespace DevOpcua {

/**
 * @brief The DataElementUaSdk implementation of a single piece of data.
 *
 * See DevOpcua::DataElement
 */
class DataElementUaSdk : public DataElement
{
public:
    /**
     * @brief Constructor for DataElement (implementation).
     * See DevOpcua::DataElement::DataElement
     *
     * @param name  structure element name (empty otherwise)
     */
    DataElementUaSdk(const std::string &name = "");

    /**
     * @brief Push an incoming data value into the DataElement.
     *
     * Called from the OPC UA client worker thread when new data is
     * received from the OPC UA session.
     *
     * @param value  new value for this data element
     */
    void setIncomingData(const UaDataValue &value);

    /**
     * @brief Read the time stamp of the incoming data.
     * See DevOpcua::DataElement::readTimeStamp
     *
     * @param server  true = server time stamp
     * @return EPICS time stamp
     */
    epicsTimeStamp readTimeStamp(bool server = true) const override;

    /**
     * @brief Read incoming data as Int32. See DevOpcua::DataElement::readInt32
     *
     * @return value as epicsInt32
     * @throws std::runtime_error if no data present or on conversion error
     */
    epicsInt32 readInt32() const override;

    /**
     * @brief Read incoming data as UInt32. See DevOpcua::DataElement::readUInt32
     *
     * @return value as epicsUInt32
     * @throws std::runtime_error if no data present or on conversion error
     */
    epicsUInt32 readUInt32() const override;

    /**
     * @brief Read incoming data as Float64. See DevOpcua::DataElement::readFloat64
     *
     * @return value as epicsFloat64
     * @throws std::runtime_error if no data present or on conversion error
     */
    epicsFloat64 readFloat64() const override;
//    epicsOldString readOldString() const override;

    /**
     * @brief Clear (discard) the current incoming data.
     * See DevOpcua::DataElement::clearIncomingData
     */
    void clearIncomingData() override { incomingData.clear(); }

private:
    UaDataValue incomingData;        /**< incoming data value */
    OpcUa_BuiltInType incomingType;  /**< type of incoming data */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTUASDK_H
