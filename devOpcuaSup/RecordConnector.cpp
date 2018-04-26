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

#include <epicsExport.h>

#include "RecordConnector.h"

namespace DevOpcua {

// Configurable default for timestamp selection

static int opcua_DefaultUseServerTime = 1;           // use server timestamp

extern "C"{
epicsExportAddress(int, opcua_DefaultUseServerTime);
}

RecordConnector::RecordConnector(dbCommon* prec)
    : prec(prec)
{}

} // namespace DevOpcua
