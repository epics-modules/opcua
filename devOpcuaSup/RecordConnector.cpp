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

#include "RecordConnector.h"

namespace DevOpcua {

RecordConnector::RecordConnector(dbCommon* prec)
    : prec(prec)
{

}

template<typename VAL>
VAL read()
{
    epicsUInt32 OV(0);
    return OV;
}

template<typename VAL>
void write(VAL val)
{

}

} // namespace DevOpcua
