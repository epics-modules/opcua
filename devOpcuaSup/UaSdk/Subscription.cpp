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

#include <errlog.h>

#define epicsExportSharedSymbols
#include "Session.h"
#include "Subscription.h"
#include "SessionUaSdk.h"
#include "SubscriptionUaSdk.h"

namespace DevOpcua {

Subscription::~Subscription() {}

void
Subscription::createSubscription (const std::string &name, const std::string &session,
                                  const double publishingInterval, const epicsUInt8 priority,
                                  const int debug)
{
    new SubscriptionUaSdk(name, &(SessionUaSdk::findSession(session)),
                          publishingInterval, priority, debug);
}

Subscription &
Subscription::findSubscription (const std::string &name)
{
    return static_cast<Subscription &>(SubscriptionUaSdk::findSubscription(name));
}

bool
Subscription::subscriptionExists (const std::string &name)
{
    return SubscriptionUaSdk::subscriptionExists(name);
}

void
Subscription::showAll (const int level)
{
    SubscriptionUaSdk::showAll(level);
}

} // namespace DevOpcua
