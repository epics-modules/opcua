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

namespace DevOpcua {

class RecordConnector;

class DataElement
{
public:
    DataElement(const std::string &name = "");
    virtual ~DataElement();

    void setRecordConnector(RecordConnector *connector);

protected:
    std::string name;                                    /**< element name */
    std::vector<std::unique_ptr<DataElement>> elements;  /**< children (if node) */
    RecordConnector *pconnector;                         /**< connector (if leaf) */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENT_H
