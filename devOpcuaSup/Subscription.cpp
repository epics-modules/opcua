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

#include "Subscription.h"

namespace DevOpcua {

// Configurable default for publishing interval

static double opcua_DefaultPublishInterval = 100.0;  // ms

extern "C" {
epicsExportAddress(double, opcua_DefaultPublishInterval);
}

using namespace UaClientSdk;

Subscription::Subscription()
{

}

} // namespace DevOpcua
