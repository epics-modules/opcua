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

#ifndef DEVOPCUA_SESSION_H
#define DEVOPCUA_SESSION_H

#include <iostream>
#include <map>

#include <epicsTypes.h>
#include <initHooks.h>

namespace DevOpcua {

/**
 * @brief The Session class manages a session with an OPC UA Server.
 *
 * Main class for connecting with any OPC UA Server.
 * The class manages the connection to an OPC Unified Architecture server
 * and the application session established with the server.
 *
 * The connect call establishes and maintains a Session with a Server.
 * After a successful connect, the connection is monitored by the low level driver.
 *
 * The disconnect call disconnects the Session, deleting all Subscriptions
 * and freeing all related resources on both server and client.
 */

class Session
{
public:
    /**
     * @brief Create an OPC UA session.
     *
     * @param name               session name (used in EPICS record configuration)
     * @param debug              initial debug verbosity level
     * @param autoConnect        automatic session (re)connect
     */
    Session(const std::string &name, const int debug, const bool autoConnect);
    virtual ~Session() {}

    /**
     * @brief Connect the underlying OPC UA Session.
     *
     * Try connecting the session to the OPC UA server.
     *
     * Non-blocking. Connection status changes will be reported through the
     * UaClientSdk::UaSessionCallback interface.
     *
     * If the server is not available at the time of calling,
     * the client library will continue trying to connect.
     *
     * @return long status. 0 = OK
     */
    virtual long connect() = 0;

    /**
     * @brief Disconnect the underlying OPC UA Session.
     *
     * Disconnect the session from the OPC UA server.
     *
     * This will delete all subscriptions related to the session on both client
     * and server side, and free all connected resources.
     * The disconnect will complete and the status is changed to disconnected even
     * if the underlying service fails and a bad status is returned.
     *
     * The call will block until all outstanding service calls and active
     * client-side callbacks have been completed.
     * Those are not allowed to block, else client deadlocks will appear.
     *
     * Connection status changes will be reported through the
     * UaClientSdk::UaSessionCallback interface.
     *
     * @return long status. 0 = OK
     */
    virtual long disconnect() = 0;

    /**
     * @brief Return connection status of the underlying OPC UA Session.
     *
     * Return the session's connection status.
     *
     * @return bool connection status.
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Print configuration and status on stdout.
     *
     * @param level  verbosity level (0 = one line; 1 = one line per session)
     */
    virtual void show(int level) const = 0;

    /**
     * @brief Print configuration and status of all sessions on stdout.
     *
     * @param level  verbosity level (0 = one line; 1 = one line per session)
     */
    static void showAll(int level);

    /**
     * @brief Set the verbosity level for Session debugging.
     *
     * @param level  new verbosity level
     */
    void setDebug(int level) { debug = level; }

    /**
     * @brief Set the debug verbosity level for all sessions.
     *
     * @param level  new verbosity level
     */
    static void setDebugAll(int level);

    /**
     * @brief Find a session by name.
     *
     * @param name session name
     *
     * @return Session & session
     */
    static Session & findSession(const std::string &name);

    /**
     * @brief Check if a session with the specified name exists.
     *
     * @param name session name
     *
     * @return bool
     */
    static bool sessionExists(const std::string &name);

    /**
     * @brief EPICS IOC Database initHook function.
     *
     * Hook function called when the EPICS IOC is being initialized.
     * Connects all sessions with autoConnect=true.
     *
     * @param state  initialization state
     */
    static void initHook(initHookState state);

    /**
     * @brief EPICS IOC Database atExit function.
     *
     * Hook function called when the EPICS IOC is exiting.
     * Disconnects all sessions.
     */
    static void atExit(void *junk);

protected:
    const std::string name; /**< session name */
    int debug;              /**< debug verbosity level */
    bool autoConnect;       /**< auto (re)connect flag */

private:
    static std::map<std::string, Session*> sessions;
};

} // namespace DevOpcua

#endif // DEVOPCUA_SESSION_H
