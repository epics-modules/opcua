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

#include "DataElement.h"
#include "RecordConnector.h"

namespace DevOpcua {

DataElement::DataElement (const std::string &name)
    : name(name)
{}

DataElement::~DataElement ()
{
    if (pconnector)
        pconnector->clearDataElement();
}

void
DataElement::setRecordConnector (RecordConnector *connector)
{
    if (pconnector)
        pconnector->clearDataElement();
    pconnector = connector;
    connector->setDataElement(this);
}

} // namespace DevOpcua
