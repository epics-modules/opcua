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

#ifndef DEVOPCUA_SUBSCRIPTIONOPEN62541_H
#define DEVOPCUA_SUBSCRIPTIONOPEN62541_H

#include <epicsTypes.h>

#include "SessionOpen62541.h"
#include "Subscription.h"
#include "Registry.h"

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
    // Cannot copy a subscription
    SubscriptionOpen62541(const SubscriptionOpen62541 &);
    SubscriptionOpen62541 &operator=(const SubscriptionOpen62541 &);


public:
    /**
     * @brief Constructor for SubscriptionOpen62541.
     *
     * @param name  subscription name
     * @param session  session that the subscription should be linked to
     * @param publishingInterval  initial publishing interval
     */
    SubscriptionOpen62541(const std::string &name,
                          SessionOpen62541 &session,
                          const double publishingInterval);


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
    static SubscriptionOpen62541 *
    find(const std::string &name)
    {
        return subscriptions.find(name);
    }

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

    // SubscriptionCallback interface
    void subscriptionStatusChanged(
            UA_StatusCode   status
            );

    void dataChange(
            UA_UInt32       monitorId,
            ItemOpen62541   &item,
            UA_DataValue    *value
            );

private:
    static Registry<SubscriptionOpen62541> subscriptions; /**< subscription management */
    SessionOpen62541 &session;                            /**< reference to session */
    std::vector<ItemOpen62541 *> items;                   /**< items on this subscription */
    UA_CreateSubscriptionResponse subscriptionSettings;   /**< subscription specific settings */
    UA_CreateSubscriptionRequest requestedSettings;       /**< requested subscription specific settings */
    bool enable;                                          /**< subscription enable flag */
};

} // namespace DevOpcua

#endif // DEVOPCUA_SUBSCRIPTIONOPEN62541_H
