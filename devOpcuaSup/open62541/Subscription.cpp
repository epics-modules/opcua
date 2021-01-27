/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#include <errlog.h>

#define epicsExportSharedSymbols
#include "Session.h"
#include "Subscription.h"
#include "SessionOpen62541.h"
#include "SubscriptionOpen62541.h"

namespace DevOpcua {

Subscription::~Subscription() {}

void
Subscription::createSubscription (const std::string &name, const std::string &session,
                                  const double publishingInterval, const epicsUInt8 priority,
                                  const int debug)
{
    new SubscriptionOpen62541(name, &(SessionOpen62541::findSession(session)),
                          publishingInterval, priority, debug);
}

Subscription &
Subscription::findSubscription (const std::string &name)
{
    return static_cast<Subscription &>(SubscriptionOpen62541::findSubscription(name));
}

bool
Subscription::subscriptionExists (const std::string &name)
{
    return SubscriptionOpen62541::subscriptionExists(name);
}

void
Subscription::showAll (const int level)
{
    SubscriptionOpen62541::showAll(level);
}

} // namespace DevOpcua
