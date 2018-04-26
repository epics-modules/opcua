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

#include <epicsExport.h>

#include "Item.h"

namespace DevOpcua {

// Configurable defaults for sampling interval, queue size, discard policy

static double opcua_DefaultSamplingInterval = -1.0;  // ms (-1 = use publishing interval)
static int opcua_DefaultQueueSize = 1;               // no queueing
static int opcua_DefaultDiscardOldest = 1;           // discard oldest value in case of overrun

extern "C" {
epicsExportAddress(double, opcua_DefaultSamplingInterval);
epicsExportAddress(int, opcua_DefaultQueueSize);
epicsExportAddress(int, opcua_DefaultDiscardOldest);
}

using namespace UaClientSdk;

Item::Item()
{

}

} // namespace DevOpcua
