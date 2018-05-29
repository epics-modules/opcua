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
#include <errlog.h>

#include "Session.h"
#include "Subscription.h"

namespace DevOpcua {

//TODO: Subscription::findSubscription() and Subscription::subscriptionExists()
// are currently implemented in the (only) implementation SubscriptionUaSdk.
// This should be made dynamic (the subscription class should manage its implementations).

Subscription::~Subscription() {}

} // namespace DevOpcua
