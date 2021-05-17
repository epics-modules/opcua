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
#include <fstream>
#include <cstdio>

#include <uaplatformlayer.h>
#include <uabase.h>
#ifdef HAS_SECURITY
#include <uapkicertificate.h>
#endif

#include <epicsThread.h>
#include <errlog.h>

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

static bool
isWritable(const std::string &dir)
{
    char filename[L_tmpnam];
    std::tmpnam(filename);
    std::string uniqname(filename);
    bool writable = false;

    std::string testfile = dir;
    if (testfile.back() == pathsep)
        testfile.pop_back();

    size_t pos = uniqname.find_last_of(pathsep);
    testfile.append(uniqname, pos, uniqname.length() - pos);

    std::ofstream file(testfile);
    if ((file.rdstate() & std::fstream::failbit) == 0) {
        writable = true;
        file.close();
        remove(testfile.c_str());
    }
    return writable;
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

std::set<Session *>
Session::glob(const std::string &pattern)
{
    return SessionUaSdk::glob(pattern);
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
Session::showClientSecurity()
#ifdef HAS_SECURITY
{
    ClientSecurityInfo securityInfo;

    SessionUaSdk::setupClientSecurityInfo(securityInfo);

    std::cout << "Certificate store:"
              << "\n  Server trusted certificates dir: " << securityCertificateTrustListDir
              << "\n  Server revocation list dir: " << securityCertificateRevocationListDir
              << "\n  Issuer trusted certificates dir: " << securityIssuersCertificatesDir
              << "\n  Issuer revocation list dir: " << securityIssuersRevocationListDir;
    if (securitySaveRejected)
        std::cout << "\n  Rejected certificates saved to: " << securitySaveRejectedDir;
    else
        std::cout << "\n  Rejected certificates are not saved.";

    std::cout << "\nApplicationURI: " << applicationUri;

    if (securityInfo.clientCertificate.length() > 0) {
        UaPkiCertificate cert = UaPkiCertificate::fromDER(securityInfo.clientCertificate);
        UaPkiIdentity id = cert.subject();
        std::cout << "\nClient certificate: " << id.commonName.toUtf8() << " ("
                  << id.organization.toUtf8() << ")"
                  << " serial " << cert.serialNumber().toUtf8() << " (thumb "
                  << cert.thumbPrint().toHex(false).toUtf8() << ")"
                  << (cert.isSelfSigned() ? " self-signed" : "")
                  << "\n  Certificate file: " << securityClientCertificateFile
                  << "\n  Private key file: " << securityClientPrivateKeyFile;
    } else {
        std::cout << "\nNo client certificate loaded.";
    }
    std::cout << "\nSupported security policies: ";
    for (const auto &p : securitySupportedPolicies)
        std::cout << " " << p.second;
    std::cout << std::endl;
}
#else
{
    std::cout << "Client library does not support security features.";
    std::cout << "\nSupported security policies: ";
    for (const auto &p : securitySupportedPolicies)
        std::cout << " " << p.second;
    std::cout << std::endl;
}
#endif

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

void
Session::setupPKI(const std::string &&certTrustList,
                  const std::string &&certRevocationList,
                  const std::string &&issuersTrustList,
                  const std::string &&issuersRevocationList)
{
    securityCertificateTrustListDir = std::move(certTrustList);
    securityCertificateRevocationListDir = std::move(certRevocationList);
    securityIssuersCertificatesDir = std::move(issuersTrustList);
    securityIssuersRevocationListDir = std::move(issuersRevocationList);

    const std::string format(
        "OPC UA: Warning - a PKI directory is writable, which may compromise security. (%s)\n");
    if (isWritable(securityCertificateTrustListDir))
        errlogPrintf(format.c_str(), securityCertificateTrustListDir.c_str());
    if (isWritable(securityCertificateRevocationListDir))
        errlogPrintf(format.c_str(), securityCertificateRevocationListDir.c_str());
    if (isWritable(securityIssuersCertificatesDir))
        errlogPrintf(format.c_str(), securityIssuersCertificatesDir.c_str());
    if (isWritable(securityIssuersRevocationListDir))
        errlogPrintf(format.c_str(), securityIssuersRevocationListDir.c_str());
}

void
Session::saveRejected(const std::string &location)
{
    securitySaveRejected = true;
    if (location.length()) {
        securitySaveRejectedDir = location;
        if (securitySaveRejectedDir.back() == '/')
            securitySaveRejectedDir.pop_back();
    }
}

const std::string &
opcuaGetDriverName ()
{
    static const std::string sdk("Unified Automation C++ Client SDK v"
                                 str(PROD_MAJOR) "." str(PROD_MINOR) "."
                                 str(PROD_PATCH) "-" str(PROD_BUILD));
    return sdk;
}

std::string Session::hostname;
std::string Session::iocname;
std::string Session::applicationUri;
std::string Session::securityCertificateTrustListDir;
std::string Session::securityCertificateRevocationListDir;
std::string Session::securityIssuersCertificatesDir;
std::string Session::securityIssuersRevocationListDir;
std::string Session::securityClientCertificateFile;
std::string Session::securityClientPrivateKeyFile;
bool Session::securitySaveRejected;
std::string Session::securitySaveRejectedDir;

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
