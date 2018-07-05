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

#include "DataElement.h"

namespace DevOpcua {

class DataElementUaSdk : public DataElement
{
public:
    DataElementUaSdk(const std::string &name = "");
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTUASDK_H
