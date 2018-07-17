/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 */

#ifndef DEVOPCUA_DATAELEMENT_H
#define DEVOPCUA_DATAELEMENT_H

#include <vector>
#include <memory>

#include <epicsTypes.h>
#include <epicsTime.h>

namespace DevOpcua {

class RecordConnector;

class DataElement
{
public:
    DataElement(const std::string &name = "");
    virtual ~DataElement();

    void setRecordConnector(RecordConnector *connector);

    virtual epicsTimeStamp readTimeStamp(bool server = true) const = 0;

    virtual epicsInt32   readInt32() const = 0;
    virtual epicsUInt32  readUInt32() const = 0;
    virtual epicsFloat64 readFloat64() const = 0;
//    virtual epicsOldString readOldString() const = 0;

    virtual void clearIncomingData() = 0;

protected:
    std::string name;                                    /**< element name */
    std::vector<std::unique_ptr<DataElement>> elements;  /**< children (if node) */
    RecordConnector *pconnector;                         /**< connector (if leaf) */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENT_H
