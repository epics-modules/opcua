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

double opcua_ConnectTimeout = 5.0;               // [s]
int opcua_MaxOperationsPerServiceCall = 0;       // no limit (do not batch)

double opcua_DefaultPublishInterval = 100.0;     // [ms]

double opcua_DefaultSamplingInterval = -1.0;     // use publish interval [ms]
int opcua_DefaultQueueSize = 1;                  // no queueing
int opcua_DefaultDiscardOldest = 1;              // discard oldest value in case of overrun
int opcua_DefaultUseServerTime = 1;              // use server timestamp
int opcua_DefaultOutputReadback = 1;             // make outputs bidirectional

extern "C" {
epicsExportAddress(double, opcua_ConnectTimeout);
epicsExportAddress(int, opcua_MaxOperationsPerServiceCall);
epicsExportAddress(double, opcua_DefaultPublishInterval);
epicsExportAddress(double, opcua_DefaultSamplingInterval);
epicsExportAddress(int, opcua_DefaultQueueSize);
epicsExportAddress(int, opcua_DefaultDiscardOldest);
epicsExportAddress(int, opcua_DefaultUseServerTime);
epicsExportAddress(int, opcua_DefaultOutputReadback);
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
