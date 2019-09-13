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
 * @brief The SubscriptionUaSdk implementation of an OPC UA Subscription.
 *
 * See DevOpcua::Subscription
 *
 * The class provides all Subscription related services.
 */
class SubscriptionUaSdk : public UaSubscriptionCallback, public Subscription
{
    UA_DISABLE_COPY(SubscriptionUaSdk);

public:
    /**
     * @brief Constructor for SubscriptionUaSdk.
     *
     * @param name  subscription name
     * @param session  session that the subscription should be linked to
     * @param publishingInterval  initial publishing interval
     * @param priority  priority (default 0=lowest)
     * @param debug  debug verbosity (default 0=no debug)
     */
    SubscriptionUaSdk(const std::string &name, SessionUaSdk *session,
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
     * @brief Get the session. See DevOpcua::Subscription::getSession
     *
     * @return Session
     */
    virtual Session &getSession() const override;

    /**
     * @brief Get the session (implementation) that this subscription
     * is running on.
     *
     * @return SessionUaSdk  reference to session implementation
     */
    SessionUaSdk &getSessionUaSdk() const;

    /**
     * @brief Add an item (implementation) to the subscription.
     *
     * The validity of the pointer supplied as parameter is bound
     * to the life time of the ItemUaSdk object.
     * The destructor ItemUaSdk::~ItemUaSdk removes the item.
     *
     * @param item  item (implementation) to add
     */
    void addItemUaSdk(ItemUaSdk *item);

    /**
     * @brief Remove an item (implementation) from the subscription.
     *
     * The ItemUaSdk is removed from the subscription's list of
     * monitored items. It is not deleted.
     *
     * @param item  item (implementation) to remove
     */
    void removeItemUaSdk(ItemUaSdk *item);

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

private:
    static std::map<std::string, SubscriptionUaSdk*> subscriptions;

    UaSubscription *puasubscription;            /**< pointer to low level subscription */
    SessionUaSdk *psessionuasdk;                /**< pointer to session */
    std::vector<ItemUaSdk *> items;             /**< items on this subscription */
    SubscriptionSettings subscriptionSettings;  /**< subscription specific settings */
    SubscriptionSettings requestedSettings;     /**< requested subscription specific settings */
    bool enable;                                /**< subscription enable flag */
};

} // namespace DevOpcua

#endif // DEVOPCUA_SUBSCRIPTIONUASDK_H
