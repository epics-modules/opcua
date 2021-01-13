/*************************************************************************\
* Copyright (c) 2018-2021 ITER Organization.
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

#include <string>
#include <map>
#include <cstring>
#include <fstream>

#include <limits.h>
#include <unistd.h>

#include <shareLib.h>
#include <epicsTimer.h>
#include <errlog.h>

#include "iocshVariables.h"

#ifndef HOST_NAME_MAX
  #define HOST_NAME_MAX 256
#endif

namespace DevOpcua {

/**
 * @brief The Session interface to the client side OPC UA session.
 *
 * Main interface for connecting with any OPC UA Server.
 * The implementation manages the connection to an OPC Unified Architecture
 * server and the application session established with it.
 *
 * The connect call establishes and maintains a Session with a Server.
 *
 * The disconnect call disconnects the Session, deleting all Subscriptions
 * and freeing all related resources on both server and client.
 */

class epicsShareClass Session
{
public:
    virtual ~Session() {}

    /**
     * @brief Connect the underlying OPC UA Session.
     *
     * Try connecting the session to the OPC UA server.
     *
     * Non-blocking. Connection status changes shall be reported through a
     * callback interface.
     *
     * If the connection attempt fails and the autoConnect flag is true,
     * the reconnect timer shall be restarted.
     *
     * @return long status (0 = OK)
     */
    virtual long connect() = 0;

    /**
     * @brief Disconnect the underlying OPC UA Session.
     *
     * Disconnect the session from the OPC UA server.
     *
     * This shall delete all subscriptions related to the session on both client
     * and server side, and free all connected resources.
     * The disconnect shall complete and the status change to disconnected even
     * if the underlying service fails and a bad status is returned.
     *
     * The call shall block until all outstanding service calls and active
     * client-side callbacks have been completed.
     * Those are not allowed to block, else client deadlocks will appear.
     *
     * Connection status changes shall be reported through a callback
     * interface.
     *
     * @return long status (0 = OK)
     */
    virtual long disconnect() = 0;

    /**
     * @brief Return connection status of the underlying OPC UA Session.
     *
     * @return bool connection status
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get session name.
     *
     * @return const std::string & name
     */
    virtual const std::string & getName() const = 0;

    /**
     * @brief Print configuration and status on stdout.
     *
     * The verbosity level controls the amount of information:
     * 0 = one line
     * 1 = session line, then one line per subscription
     *
     * @param level  verbosity level
     */
    virtual void show(const int level) const = 0;

    /**
     * @brief Print configuration and status of all sessions on stdout.
     *
     * The verbosity level controls the amount of information:
     * 0 = one summary
     * 1 = one line per session
     * 2 = one session line, then one line per subscription
     *
     * @param level  verbosity level
     */
    static void showAll(const int level);

    /**
     * @brief Do a discovery and show the available endpoints.
     */
    virtual void showSecurity() = 0;

    /**
     * @brief Show client security setup and certificate info.
     */
    static void showClientSecurity();

    /**
     * @brief Set an option for the session.
     *
     * @param name   option name
     * @param value  value
     */
    virtual void setOption(const std::string &name, const std::string &value) = 0;

    /**
     * @brief Add namespace index mapping (local index -> URI).
     *
     * @param nsIndex  local namespace index
     * @param uri  namespace URI
     */
    virtual void addNamespaceMapping(const unsigned short nsIndex, const std::string &uri) = 0;

    /**
     * @brief Factory method to create a session (implementation specific).
     *
     * @param name         name of the new session
     * @param url          url of the server to connect
     * @param debuglevel   initial debug level
     * @param autoconnect  connect automatically at IOC init
     *
     * @return  pointer to the new session, nullptr if not created
     */
    static Session *createSession(const std::string &name,
                                  const std::string &url,
                                  const int debuglevel,
                                  const bool autoconnect);

    /**
     * @brief Find a session by name (implementation specific).
     *
     * @param name  session name to search for
     *
     * @return  pointer to session, nullptr if not found
     */
    static Session *find(const std::string &name);

    /**
     * @brief Print help text for available options.
     */
    static void showOptionHelp();

    /**
     * @brief Set client certificate (public key, private key) file paths.
     *
     * @param pubPath  full path to certificate (public key)
     * @param prvPath  full path to private key file
     */
    static void setClientCertificate(const std::string &&pubKey, const std::string &&prvKey)
    {
        securityClientCertificateFile = std::move(pubKey);
        securityClientPrivateKeyFile = std::move(prvKey);
    }

    /**
     * @brief Setup client PKI (cert store locations).
     *
     * @param certTrustList  server certificate location
     * @param certRevocationList  server revocation list location
     * @param issuersTrustList  issuers certificate location
     * @param issuersRevocationList  issuers revocation list location
     */
    static void setupPKI(const std::string &&certTrustList,
                         const std::string &&certRevocationList,
                         const std::string &&issuersTrustList,
                         const std::string &&issuersRevocationList);

    /**
     * @brief Enable saving of rejected (untrusted) server certificates.
     *
     * @param location  location for saving rejected certs (empty = use default)
     */
    static void saveRejected(const std::string &location = "");

    int debug;  /**< debug verbosity level */

protected:
    Session(const std::string &name, const int debug, const bool autoConnect)
        : debug(debug)
        , name(name)
        , autoConnector(*this, opcua_ConnectTimeout, queue)
        , autoConnect(autoConnect)
    {
        char host[HOST_NAME_MAX] = {0};
        int status = gethostname(host, sizeof(host));
        if (status)
            strcpy(host, "unknown-host");
        hostname = host;

        const char *ioc = std::getenv("IOC");
        if (!ioc)
            ioc = "ioc";
        iocname = ioc;
    }

    static std::string hostname;
    static std::string iocname;
    static std::string securityCertificateTrustListDir;  /**< trusted server certs location */
    static std::string
        securityCertificateRevocationListDir;            /**< server cert revocation lists location */
    static std::string securityIssuersCertificatesDir;   /**< trusted issuer certs location */
    static std::string securityIssuersRevocationListDir; /**< issuer cert revocation lists location */
    static std::string securityClientCertificateFile;    /**< full path to the client cert (public key) */
    static std::string securityClientPrivateKeyFile;     /**< full path to the client private key */
    static bool securitySaveRejected;                    /**< flag: save rejected (untrusted) certs */
    static std::string securitySaveRejectedDir;          /**< rejected certificates location */

    /**
     * @brief std::map of (URI, name) pairs for all supported security policies.
     */
    static const std::map<std::string, std::string> securitySupportedPolicies;
    /** @brief URI to name converter. */
    static const std::string securityPolicyString(const std::string &policy);

    /**
     * @brief Determine if a file contains PEM encoded keys or certificates.
     *
     * @param file  file name
     *
     * @return true if file uses PEM format
     */
    static bool
    isPEM(const std::string &file)
    {
        if (file.length()) {
            std::ifstream inFile;
            std::string line;
            bool foundPemMarker = false;

            inFile.open(file);
            if (inFile.fail()) {
                errlogPrintf("OPC UA: cannot open file %s\n", file.c_str());
                return false;
            }

            while (std::getline(inFile, line)) {
                size_t cert = line.find("-----BEGIN CERTIFICATE-----");
                size_t key = line.find("-----BEGIN RSA PRIVATE KEY-----");
                if (!(cert && key))
                    foundPemMarker = true;
            }
            return foundPemMarker;
        } else {
            return false;
        }
    }

    /** @brief Delay timer for reconnecting whenever connection is down. */
    class AutoConnect : public epicsTimerNotify {
    public:
        AutoConnect(Session &client, const double delay, epicsTimerQueueActive &queue)
            : timer(queue.createTimer())
            , client(client)
            , delay(delay)
        {}
        virtual ~AutoConnect() override { timer.destroy(); }
        void start() { timer.start(*this, delay); }
        virtual expireStatus expire(const epicsTime &/*currentTime*/) override {
            client.connect();
            return expireStatus(noRestart); // client.connect() starts the timer on failure
        }
    private:
        epicsTimer &timer;
        Session &client;
        const double delay;
    };

    static epicsTimerQueueActive &queue;   /**< timer queue for session reconnects */

    const std::string name;                /**< unique session name */
    AutoConnect autoConnector;             /**< reconnection timer */
    bool autoConnect;                      /**< auto (re)connect flag */
    std::string securityIdentityFile;      /**< full path to file with Identity token settings */
    std::string securityUserName;          /**< user name set in Username token */
};


} // namespace DevOpcua

#endif // DEVOPCUA_SESSION_H
