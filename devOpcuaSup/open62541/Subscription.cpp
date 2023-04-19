/*************************************************************************\
* Copyright (c) 2018-2021 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#include <errlog.h>

#include <epicsThread.h>
#include <epicsEvent.h>

#define epicsExportSharedSymbols
#include "Session.h"
#include "Subscription.h"
#include "SessionOpen62541.h"
#include "SubscriptionOpen62541.h"

namespace DevOpcua {

Subscription::~Subscription() {}

Subscription *
Subscription::createSubscription(const std::string &name,
                                 const std::string &session,
                                 const double publishingInterval,
                                 const epicsUInt8 priority,
                                 const int debug)
{
    SessionOpen62541 *s = SessionOpen62541::find(session);
    if (RegistryKeyNamespace::global.contains(name) || !s)
        return nullptr;
    return new SubscriptionOpen62541(name, *s, publishingInterval, priority, debug);
}

Subscription *
Subscription::find (const std::string &name)
{
    return SubscriptionOpen62541::find(name);
}

std::set<Subscription *>
Subscription::glob(const std::string &pattern)
{
    return SubscriptionOpen62541::glob(pattern);
}

void
Subscription::showAll (const int level)
{
    SubscriptionOpen62541::showAll(level);
}

} // namespace DevOpcua
