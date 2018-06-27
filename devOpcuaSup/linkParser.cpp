/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 *  and code by Michael Davidsaver <mdavidsaver@ospreydcs.com>
 */

#include <memory>
#include <iostream>
#include <string>
#include <cstring>
#include <cstddef>
#include <algorithm>

#include <dbCommon.h>
#include <epicsStdlib.h>
#include <link.h>

#include "devOpcua.h"
#include "linkParser.h"
#include "iocshVariables.h"
#include "Subscription.h"
#include "Session.h"

namespace DevOpcua {

std::unique_ptr<linkInfo>
parseLink(dbCommon *prec, DBEntry &ent)
{
    const char *s;
    std::unique_ptr<linkInfo> pinfo (new linkInfo);
    DBLINK *link = ent.getDevLink();
    int debug = prec->tpro;

    if (link->type != INST_IO)
        throw std::logic_error("link is not INST_IO");

    pinfo->isOutput = ent.isOutput();

    if (debug > 4)
        std::cerr << prec->name << " parsing info items" << std::endl;

    // set default from variables and info items
    s = ent.info("opcua:SAMPLING", "");
    if (debug > 9 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:SAMPLING'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->samplingInterval = opcua_DefaultSamplingInterval;
    else
        if (epicsParseDouble(s, &pinfo->samplingInterval, NULL))
            throw std::runtime_error(SB() << "error converting '" << s << "' to Double");

    s = ent.info("opcua:QSIZE", "");
    if (debug > 9 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:QSIZE'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->queueSize = opcua_DefaultQueueSize;
    else
        if (epicsParseUInt32(s, &pinfo->queueSize, 0, NULL))
            throw std::runtime_error(SB() << "error converting '" << s << "' to UInt32");

    s = ent.info("opcua:DISCARD", "");
    if (debug > 9 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:DISCARD'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->discardOldest = !!opcua_DefaultDiscardOldest;
    else
        if (strcmp(s, "new") == 0)
            pinfo->discardOldest = false;
        else if (strcmp(s, "old") == 0)
            pinfo->discardOldest = true;
        else
            throw std::runtime_error(SB() << "illegal value '" << s << "'");

    s = ent.info("opcua:TIMESTAMP", "");
    if (debug > 9 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:TIMESTAMP'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->useServerTimestamp = !!opcua_DefaultUseServerTime;
    else
        if (strcmp(s, "server") == 0)
            pinfo->useServerTimestamp = true;
        else if (strcmp(s, "source") == 0)
            pinfo->useServerTimestamp = false;
        else
            throw std::runtime_error(SB() << "illegal value '" << s << "'");

    s = ent.info("opcua:READBACK", "");
    if (debug > 9 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:READBACK'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->doOutputReadback = !!opcua_DefaultOutputReadback;
    else
        if (strchr("YyTt1", s[0]))
            pinfo->doOutputReadback = true;
        else if (strchr("NnFf0", s[0]))
            pinfo->doOutputReadback = false;
        else
            throw std::runtime_error(SB() << "illegal value '" << s << "'");

    s = ent.info("opcua:ELEMENT", "");
    if (debug > 9 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:ELEMENT'='" << s << "'" << std::endl;
    if (s[0] != '\0')
        pinfo->element = s;

    // parse INP/OUT link
    std::string linkstr(link->text);
    if (debug > 4)
        std::cerr << prec->name << " parsing inp/out link '" << linkstr << "'" << std::endl;

    // first token: session or subscription name
    size_t sep = linkstr.find_first_of("@ \t");
    std::string name = linkstr.substr(0, sep);

    if (Subscription::subscriptionExists(name)) {
        pinfo->subscription = name;
    } else if (Session::sessionExists(name)) {
        pinfo->session = name;
    } else {
        throw std::runtime_error(SB() << "unknown session or subscription '" << name << "'");
    }

    sep = linkstr.find_first_not_of("@ \t", sep);

    // everything else is "key=value ..." options
    while (sep < linkstr.size()) {
        size_t send = linkstr.find_first_of("; \t", sep),
               seq  = linkstr.find_first_of('=', sep);

        if (seq >= send)
            throw std::runtime_error(SB() << "expected '=' in '" << linkstr.substr(0, send) << "'");

        std::string optname(linkstr.substr(sep, seq-sep)),
                    optval (linkstr.substr(seq+1, send-seq-1));

        if (debug > 9) {
            std::cerr << prec->name << " opt '" << optname << "'='" << optval << "'" << std::endl;
        }

        if (optname == "ns") {
            if (epicsParseUInt16(optval.c_str(), &pinfo->namespaceIndex, 0, NULL))
                throw std::runtime_error(SB() << "error converting '" << optval << "' to UInt16");
        } else if (optname == "s") {
            pinfo->identifierString = optval;
            pinfo->identifierIsNumeric = false;
        } else if (optname == "i") {
            if (epicsParseUInt32(optval.c_str(), &pinfo->identifierNumber, 0, NULL))
                throw std::runtime_error(SB() << "error converting '" << optval << "' to UInt32");
            pinfo->identifierIsNumeric = true;
        } else if (optname == "sampling") {
            if (epicsParseDouble(optval.c_str(), &pinfo->samplingInterval, NULL))
                throw std::runtime_error(SB() << "error converting '" << optval << "' to Double");
        } else if (optname == "qsize") {
            if (epicsParseUInt32(optval.c_str(), &pinfo->queueSize, 0, NULL))
                throw std::runtime_error(SB() << "error converting '" << optval << "' to UInt32");
        } else if (optname == "discard") {
            if (optval == "new")
                pinfo->discardOldest = false;
            else if (optval == "old")
                pinfo->discardOldest = true;
            else
                throw std::runtime_error(SB() << "illegal value '" << optval << "'");
        } else if (optname == "timestamp") {
            if (optval == "server")
                pinfo->useServerTimestamp = true;
            else if (optval == "source")
                pinfo->useServerTimestamp = false;
            else
                throw std::runtime_error(SB() << "illegal value '" << optval << "'");
        } else if (optname == "readback") {
            if (optval.length() > 0) {
                char c = optval[0];
                if (strchr("YyTt1", c))
                    pinfo->doOutputReadback = true;
                else if (strchr("NnFf0", c))
                    pinfo->doOutputReadback = false;
                else
                    throw std::runtime_error(SB() << "illegal value '" << optval << "'");
            } else {
                throw std::runtime_error(SB() << "no value for option '" << optname << "'");
            }
        } else if (optname == "element") {
            pinfo->element = optval;
        } else {
            throw std::runtime_error(SB() << "unknown option '" << optname << "'");
        }

        sep = linkstr.find_first_not_of("; \t", send);
    }

    if (debug > 4) {
        std::cerr << prec->name << " :";
        if (pinfo->session.length())
            std::cerr << " session=" << pinfo->session;
        else if (pinfo->subscription.length())
            std::cerr << " subscription=" << pinfo->subscription;
        std::cerr << " ns=" << pinfo->namespaceIndex;
        if (pinfo->identifierIsNumeric)
            std::cerr << " id(i)=" << pinfo->identifierNumber;
        else
            std::cerr << " id(s)=" << pinfo->identifierString;
        std::cerr << " sampling=" << pinfo->samplingInterval
                  << " qsize=" << pinfo->queueSize
                  << " discard=" << (pinfo->discardOldest ? "old" : "new")
                  << " timestamp=" << (pinfo->useServerTimestamp ? "server" : "source");
        if (pinfo->isOutput)
            std::cerr << " output=y readback=" << (pinfo->doOutputReadback ? "y" : "n");
        std::cerr << std::endl;
    }

    // consistency checks
    if (pinfo->isOutput && pinfo->doOutputReadback && !pinfo->subscription.length())
        throw std::runtime_error(SB() << "readback of output requires a valid subscription");

    return pinfo;
}

} // namespace DevOpcua
