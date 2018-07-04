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

#ifndef DEVOPCUA_SUBSCRIPTION_H
#define DEVOPCUA_SUBSCRIPTION_H

#include <iostream>
#include <map>

namespace DevOpcua {

class Session;

/**
 * @brief The Subscription interface for a UA client created subscription.
 *
 * The interface provides all subscription related UA services.
 */

class Subscription
{
public:
    virtual ~Subscription();

    /**
     * @brief Print configuration and status on stdout.
     *
     * The verbosity level controls the amount of information:
     * 0 = one line
     *
     * @param level  verbosity level
     */
    virtual void show(int level) const = 0;

    /**
     * @brief Get the session that this subscription is running on.
     *
     * @return Session
     */
    virtual Session & getSession() const = 0;

    /**
     * @brief Find a subscription by name.
     *
     * @param name subscription name
     *
     * @return Subscription & subscription
     */
    static Subscription & findSubscription(const std::string &name);

    /**
     * @brief Check if a subscription with the specified name exists.
     *
     * @param name subscription name
     *
     * @return bool
     */
    static bool subscriptionExists(const std::string &name);

    const std::string name; /**< subscription name */
    int debug;              /**< debug verbosity level */

protected:
    // No construction through the interface
    Subscription(const int debug) : debug(debug) {}
};

} // namespace DevOpcua

#endif // DEVOPCUA_SUBSCRIPTION_H
