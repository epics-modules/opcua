/*************************************************************************\
* Copyright (c) 2018-2023 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#include <iostream>
#include <epicsThread.h>

#define epicsExportSharedSymbols
#include "Session.h"
#include "SessionOpen62541.h"
#include "Registry.h"

namespace DevOpcua {

epicsThreadOnceId Session::onceId = EPICS_THREAD_ONCE_INIT;
epicsTimerQueueActive *Session::queue = nullptr;

RegistryKeyNamespace RegistryKeyNamespace::global;

void Session::initOnce(void *)
{
    queue = &epicsTimerQueueActive::allocate(true);
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
                       const std::string &url)
{
    epicsThreadOnce(&onceId, &initOnce, nullptr);
    if (RegistryKeyNamespace::global.contains(name))
        return nullptr;
    return new SessionOpen62541(name, url);
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

const std::string
Session::securityPolicyString(const std::string &policy)
{
    if (!policy.length())
        return "None";
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

    SessionOpen62541::setupClientSecurityInfo(securityInfo);

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

    if (securityInfo.clientCertificate.length > 0) {
        //UA_ByteString cert = securityInfo.clientCertificate;
        //UaPkiIdentity id = cert.subject();
        std::cout << "\nClient certificate: " // << id.commonName << " ("
                //  << id.organization << ")"
                //  << " serial " << cert.serialNumber() << " (thumb "
                //  << cert.thumbPrint().toHex(false) << ")"
                //  << (cert.isSelfSigned() ? " self-signed" : "")
                  << "\n  Certificate file: " << securityClientCertificateFile
                  << "\n  Private key file: " << securityClientPrivateKeyFile;
    } else
    {
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

const char Session::optionUsage[]
    = "Sets options for existing OPC UA sessions or subscriptions.\n\n"
      "pattern    pattern for session or subscription names (* and ? supported)\n"
      "[options]  colon separated list of options in 'key=value' format\n\n"
      "Valid session options are:\n"
      "debug              debug level [default 0 = no debug]\n"
      "autoconnect        automatically connect sessions [default y]\n"
      "nodes-max          max. nodes per service call [0 = no limit]\n"
      "read-nodes-max     max. nodes per read service call [0 = no limit]\n"
      "read-timeout-min   min. timeout (holdoff) after read service call [ms]\n"
      "read-timeout-max   timeout (holdoff) after read service call w/ max elements [ms]\n"
      "write-nodes-max    max. nodes per write service call [0 = no limit]\n"
      "write-timeout-min  min. timeout (holdoff) after write service call [ms]\n"
      "write-timeout-max  timeout (holdoff) after write service call w/ max elements [ms]\n"
      "sec-mode           requested security mode\n"
      "sec-policy         requested security policy\n"
      "ident-file         file to read identity credentials from\n\n"
      "";

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

#define OPEN62541_VERSION(MAJOR, MINOR, PATCH) "v" str(MAJOR) "." str(MINOR) "." str(PATCH)
#define str(s) #s

const std::string &
opcuaGetDriverName ()
{
    static const std::string version(
        "Open62541 Client API " OPEN62541_VERSION(UA_OPEN62541_VER_MAJOR,
                                                  UA_OPEN62541_VER_MINOR,
                                                  UA_OPEN62541_VER_PATCH));
    return version;
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
    = {{"http://opcfoundation.org/UA/SecurityPolicy#None",                  "None"}
#ifdef HAS_SECURITY
     , {"http://opcfoundation.org/UA/SecurityPolicy#Basic128Rsa15",         "Basic128Rsa15"}
     , {"http://opcfoundation.org/UA/SecurityPolicy#Basic256",              "Basic256"}
     , {"http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256",        "Basic256Sha256"}
     , {"http://opcfoundation.org/UA/SecurityPolicy#Aes128_Sha256_RsaOaep", "Aes128_Sha256_RsaOaep"}
     , {"http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss",  "Aes256_Sha256_RsaPss"}
#endif
};

} // namespace DevOpcua
