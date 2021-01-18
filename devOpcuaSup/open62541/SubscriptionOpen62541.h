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

#ifndef DEVOPCUA_SUBSCRIPTIONOPEN62541_H
#define DEVOPCUA_SUBSCRIPTIONOPEN62541_H

#include <epicsTypes.h>

#include "SessionOpen62541.h"
#include "Subscription.h"

namespace DevOpcua {

/**
 * @brief The SubscriptionOpen62541 implementation of an OPC UA Subscription.
 *
 * See DevOpcua::Subscription
 *
 * The class provides all Subscription related services.
 */
class SubscriptionOpen62541 : public Subscription
{
//    UA_DISABLE_COPY(SubscriptionOpen62541);

public:
    /**
     * @brief Constructor for SubscriptionOpen62541.
     *
     * @param name  subscription name
     * @param session  session that the subscription should be linked to
     * @param publishingInterval  initial publishing interval
     * @param priority  priority (default 0=lowest)
     * @param debug  debug verbosity (default 0=no debug)
     */
    SubscriptionOpen62541(const std::string &name, SessionOpen62541 *session,
                      const double publishingInterval, const epicsUInt8 priority = 0,
                      const int debug = 0);

    /**
     * @brief Print configuration and status. See DevOpcua::Subscription::show
     */
    virtual void show(int level) const override;

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
     * @return SubscriptionOpen62541 & subscription
     */
    static SubscriptionOpen62541 & findSubscription(const std::string &name);

    /**
     * @brief Check if a subscription with the specified name exists.
     *
     * @param name  subscription name to search for
     *
     * @return bool
     */
    static bool subscriptionExists(const std::string &name);

    /**
     * @brief Get the session. See DevOpcua::Subscription::getSession
     *
     * @return Session
     */
    virtual Session &getSession() const override;

    /**
     * @brief Get the session (implementation) that this subscription
     * is running on.
     *
     * @return SessionOpen62541  reference to session implementation
     */
    SessionOpen62541 &getSessionOpen62541() const;

    /**
     * @brief Add an item (implementation) to the subscription.
     *
     * The validity of the pointer supplied as parameter is bound
     * to the life time of the ItemOpen62541 object.
     * The destructor ItemOpen62541::~ItemOpen62541 removes the item.
     *
     * @param item  item (implementation) to add
     */
    void addItemOpen62541(ItemOpen62541 *item);

    /**
     * @brief Remove an item (implementation) from the subscription.
     *
     * The ItemOpen62541 is removed from the subscription's list of
     * monitored items. It is not deleted.
     *
     * @param item  item (implementation) to remove
     */
    void removeItemOpen62541(ItemOpen62541 *item);

    /**
     * @brief Create subscription on the server.
     *
     * If the connection to the server (session) is up, the subscription is
     * created on the server side using the createSubscription service.
     */
    void create();

    /**
     * @brief Add all monitored items of this subscription to the server.
     *
     * If the subscription is created, all monitored items (i.e. all items
     * configured to be on the subscription) are being added (created on the
     * server side) using the createMonitoredItems service.
     */
    void addMonitoredItems();

    /**
     * @brief Clear connection to driver level.
     *
     * Clears the internal pointer to the driver level subscription that was
     * created through the create() method.
     */
    void clear();

/*
    // UaSubscriptionCallback interface
    virtual void subscriptionStatusChanged(
            OpcUa_UInt32      clientSubscriptionHandle,
            const UaStatus&   status
            ) override;
    virtual void dataChange(
            OpcUa_UInt32               clientSubscriptionHandle,
            const UaDataNotifications& dataNotifications,
            const UaDiagnosticInfos&   diagnosticInfos
            ) override;
    virtual void newEvents(
            OpcUa_UInt32                clientSubscriptionHandle,
            UaEventFieldLists&          eventFieldList
            ) override;
*/

private:
    static std::map<std::string, SubscriptionOpen62541*> subscriptions;

//    UaSubscription *puasubscription;            /**< pointer to low level subscription */
    SessionOpen62541 *psessionuasdk;                /**< pointer to session */
    std::vector<ItemOpen62541 *> items;             /**< items on this subscription */
//    SubscriptionSettings subscriptionSettings;  /**< subscription specific settings */
//    SubscriptionSettings requestedSettings;     /**< requested subscription specific settings */
    bool enable;                                /**< subscription enable flag */
};

} // namespace DevOpcua

#endif // DEVOPCUA_SUBSCRIPTIONOPEN62541_H
