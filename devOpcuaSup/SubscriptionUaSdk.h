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

#ifndef DEVOPCUA_SUBSCRIPTIONUASDK_H
#define DEVOPCUA_SUBSCRIPTIONUASDK_H

#include <uabase.h>
#include <uaclientsdk.h>
#include <uasubscription.h>

#include <epicsTypes.h>

#include "SessionUaSdk.h"
#include "Subscription.h"

namespace DevOpcua {

using namespace UaClientSdk;

/**
 * @brief Subscription class for the Unified Automation OPC UA Client SDK.
 *
 * The class provides all Subscription related UA services.
 */

class SubscriptionUaSdk : public UaSubscriptionCallback, public Subscription
{
    UA_DISABLE_COPY(SubscriptionUaSdk);

public:
    SubscriptionUaSdk(const std::string &name, SessionUaSdk *session,
                      const double publishingInterval, const epicsUInt8 priority=0,
                      const int debug=0);

    void show(int level) const;

    /**
     * @brief Print configuration and status of all subscriptions on stdout.
     *
     * The verbosity level controls the amount of information:
     * 0 = one summary line
     * 1 = one line per subscription
     * 2 = one subscription line, then one line per monitored item
     *
     * @param level  verbosity level
     */
    static void showAll(int level);

    /**
     * @brief Find a subscription by name.
     *
     * @param name  subscription name to search for
     *
     * @return SubscriptionUaSdk & subscription
     */
    static SubscriptionUaSdk & findSubscription(const std::string &name);

    /**
     * @brief Check if a subscription with the specified name exists.
     *
     * @param name  subscription name to search for
     *
     * @return bool
     */
    static bool subscriptionExists(const std::string &name);

    /**
     * @brief Create subscription on the server.
     */
    void create();

    // UaSubscriptionCallback interface
    void subscriptionStatusChanged(
            OpcUa_UInt32      clientSubscriptionHandle,
            const UaStatus&   status
            );
    void dataChange(
            OpcUa_UInt32               clientSubscriptionHandle,
            const UaDataNotifications& dataNotifications,
            const UaDiagnosticInfos&   diagnosticInfos
            );
    void newEvents(
            OpcUa_UInt32                clientSubscriptionHandle,
            UaEventFieldLists&          eventFieldList
            );

private:
    static std::map<std::string, SubscriptionUaSdk*> subscriptions;

    const std::string name;                     /**< unique subscription name */
    UaSubscription *puasubscription;
    SessionUaSdk *psessionuasdk;
    SubscriptionSettings subscriptionSettings;
    bool enable;
};

} // namespace DevOpcua

#endif // DEVOPCUA_SUBSCRIPTIONUASDK_H
