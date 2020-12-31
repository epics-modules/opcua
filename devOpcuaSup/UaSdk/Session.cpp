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

#include <iostream>
#include <map>

#include <uaplatformlayer.h>
#include <uabase.h>
#include <uapkicertificate.h>

#include <epicsThread.h>

#define epicsExportSharedSymbols
#include "Session.h"
#include "SessionUaSdk.h"
#include "Registry.h"

#define st(s) #s
#define str(s) st(s)

namespace DevOpcua {

static epicsThreadOnceId opcuaUaSdk_once = EPICS_THREAD_ONCE_INIT;

RegistryKeyNamespace RegistryKeyNamespace::global;

static
void opcuaUaSdk_init (void *junk)
{
    (void)junk;
    UaPlatformLayer::init();
}

Session *
Session::createSession(const std::string &name,
                       const std::string &url,
                       const int debuglevel,
                       const bool autoconnect)
{
    epicsThreadOnce(&opcuaUaSdk_once, &opcuaUaSdk_init, nullptr);
    if (RegistryKeyNamespace::global.contains(name))
        return nullptr;
    return new SessionUaSdk(name, url, autoconnect, debuglevel);
}

Session *
Session::find(const std::string &name)
{
    return SessionUaSdk::find(name);
}

void
Session::showAll (const int level)
{
    SessionUaSdk::showAll(level);
}

const std::string
Session::securityPolicyString(const std::string &policy)
{
    auto p = securitySupportedPolicies.find(policy);
    if (p == securitySupportedPolicies.end()) {
        size_t found = policy.find_last_of('#');
        if (found == std::string::npos)
            return "Invalid";
        else
            return policy.substr(found + 1) + " (unsupported)";
    } else {
        return p->second;
    }
}

void
Session::showSecurityClient()
{
    UaStatus status;
    SessionSecurityInfo securityInfo;

    //TODO: Move these two calls into a separate function and file with Linux/Windows ifdefs
    status = securityInfo.initializePkiProviderOpenSSL(
                securityCertificateRevocationListDir.c_str(),
                securityCertificateTrustListDir.c_str(),
                securityIssuersCertificatesDir.c_str(),
                securityIssuersRevocationListDir.c_str());
    if (status.isBad())
        std::cerr << "Error initializing PKI provider" << std::endl;

    status = securityInfo.loadClientCertificateOpenSSL(
                securityClientCertificateFile.c_str(),
                securityClientPrivateKeyFile.c_str());
    if (status.isBad())
        std::cerr << "Error loading client certificate" << std::endl;

    std::cout << "Certificate store:"
              << "\n  Server trusted certificates dir: " << securityCertificateTrustListDir
              << "\n  Server revocation list dir: " << securityCertificateRevocationListDir
              << "\n  Issuer trusted certificates dir: " << securityIssuersCertificatesDir
              << "\n  Issuer revocation list dir: " << securityIssuersRevocationListDir;
    UaPkiCertificate cert = UaPkiCertificate::fromDER(securityInfo.clientCertificate);
    UaPkiIdentity id = cert.subject();
    std::cout << "\nClient certificate: " << id.commonName.toUtf8() << " ("
              << id.organization.toUtf8() << ")"
              << " serial " << cert.serialNumber().toUtf8() << " (thumb "
              << cert.thumbPrint().toHex(false).toUtf8() << ")"
              << (cert.isSelfSigned() ? " self-signed" : "")
              << "\n  Certificate file: " << securityClientCertificateFile
              << "\n  Private key file: " << securityClientPrivateKeyFile
              << "\nSupported security policies: ";
    for (const auto &p : securitySupportedPolicies)
        std::cout << " " << p.second;
    std::cout << std::endl;
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
              << "sec-mode           requested security mode\n"
              << "sec-policy         requested security policy\n"
              << "sec-level-min      requested minimal security level\n"
              << "ident-file         file to read identity credentials from\n"
              << "batch-nodes        max. nodes per service call [0 = no limit]"
              << std::endl;
}

const std::string &
opcuaGetDriverName ()
{
    static const std::string sdk("Unified Automation C++ Client SDK v"
                                 str(PROD_MAJOR) "." str(PROD_MINOR) "."
                                 str(PROD_PATCH) "-" str(PROD_BUILD));
    return sdk;
}

std::string Session::securityCertificateTrustListDir;
std::string Session::securityCertificateRevocationListDir;
std::string Session::securityIssuersCertificatesDir;
std::string Session::securityIssuersRevocationListDir;
std::string Session::securityClientCertificateFile;
std::string Session::securityClientPrivateKeyFile;

const std::map<std::string, std::string> Session::securitySupportedPolicies
    = {{OpcUa_SecurityPolicy_None, "None"}
#if OPCUA_SUPPORT_SECURITYPOLICY_BASIC128RSA15
       , {OpcUa_SecurityPolicy_Basic128Rsa15, "Basic128Rsa15"}
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_BASIC256
       , {OpcUa_SecurityPolicy_Basic256, "Basic256"}
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_BASIC256SHA256
       , {OpcUa_SecurityPolicy_Basic256Sha256, "Basic256Sha256"}
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_AES128SHA256RSAOAEP
       , {OpcUa_SecurityPolicy_Aes128Sha256RsaOaep, "Aes128_Sha256_RsaOaep"}
#endif
#if OPCUA_SUPPORT_SECURITYPOLICY_AES256SHA256RSAPSS
       , {OpcUa_SecurityPolicy_Aes256Sha256RsaPss, "Aes256_Sha256_RsaPss"}
#endif
};

epicsTimerQueueActive &Session::queue = epicsTimerQueueActive::allocate(true);

} // namespace DevOpcua
