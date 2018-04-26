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

#ifndef DEVOPCUA_SUBSCRIPTION_H
#define DEVOPCUA_SUBSCRIPTION_H

#include <uabase.h>
#include <uaclientsdk.h>

namespace DevOpcua {

using namespace UaClientSdk;

class Subscription
{
public:
    Subscription();
};

} // namespace DevOpcua

#endif // DEVOPCUA_SUBSCRIPTION_H
