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

#include <iostream>
#include <string>
#include <string.h>
#include <stdexcept>

#include <iocsh.h>
#include <errlog.h>
#include <epicsExport.h>
#include <epicsThread.h>

#include "Session.h"

namespace DevOpcua {

// Configurable defaults
// - connect timeout / reconnect attempt interval [s]
// - batch size for operations (0 = no limit, don't batch)

static double opcua_ConnectTimeout = 5.0;
static int opcua_MaxOperationsPerServiceCall = 0;

extern "C" {
epicsExportAddress(double, opcua_ConnectTimeout);
epicsExportAddress(int, opcua_MaxOperationsPerServiceCall);
}

} // namespace DevOpcua

namespace {

using namespace DevOpcua;

static
void opcuaIocshRegister ()
{
}

extern "C" {
epicsExportRegistrar(opcuaIocshRegister);
}

} // namespace
