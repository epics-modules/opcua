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

#define epicsExportSharedSymbols

#include <iostream>
#include <epicsThread.h>
#include "Session.h"
#include "SessionOpen62541.h"
#include "Registry.h"

namespace DevOpcua {

RegistryKeyNamespace RegistryKeyNamespace::global;

Session *
Session::createSession(const std::string &name,
                       const std::string &url,
                       const int debuglevel,
                       const bool autoconnect)
{
    if (RegistryKeyNamespace::global.contains(name))
        return nullptr;
    return new SessionOpen62541(name, url, autoconnect, debuglevel);
}

Session *
Session::find(const std::string &name)
{
    return SessionOpen62541::find(name);
}

std::set<Session *>
Session::glob(const std::string &pattern)
{
    return SessionOpen62541::glob(pattern);
}

void
Session::showAll (const int level)
{
    SessionOpen62541::showAll(level);
}

void
Session::showOptionHelp ()
{
    std::cout << "Options:\n"
              << "clientcert         path to client certificate [none]\n"
              << "clientkey          path to client private key [none]\n"
              << "nodes-max          max. nodes per service call [0 = no limit]\n"
              << "read-nodes-max     max. nodes per read service call [0 = no limit]\n"
              << "read-timeout-min   min. timeout (holdoff) after read service call [ms]\n"
              << "read-timeout-max   timeout (holdoff) after read service call w/ max elements [ms]\n"
              << "write-nodes-max    max. nodes per write service call [0 = no limit]\n"
              << "write-timeout-min  min. timeout (holdoff) after write service call [ms]\n"
              << "write-timeout-max  timeout (holdoff) after write service call w/ max elements [ms]"
              << std::endl;
}

const std::string &
opcuaGetDriverName ()
{
    static const std::string version("Open62541 Client API " UA_OPEN62541_VER_COMMIT);
    return version;
}

std::string Session::securityCertificateTrustListDir;
std::string Session::securityCertificateRevocationListDir;
std::string Session::securityIssuersCertificatesDir;
std::string Session::securityIssuersRevocationListDir;
std::string Session::securityClientCertificateFile;
std::string Session::securityClientPrivateKeyFile;

} // namespace DevOpcua
