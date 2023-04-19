/*************************************************************************\
* Copyright (c) 2018-2021 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on code by Michael Davidsaver <mdavidsaver@ospreydcs.com>
 */

#include <memory>
#include <iostream>
#include <string>
#include <cstring>
#include <cstddef>
#include <cmath>
#include <list>
#include <algorithm>

#include <dbCommon.h>
#include <dbAccess.h>
#include <epicsStdlib.h>
#include <link.h>
#include <epicsEvent.h>
#include <epicsThread.h>

#define epicsExportSharedSymbols
#include "devOpcua.h"
#include "linkParser.h"
#include "opcuaItemRecord.h"
#include "iocshVariables.h"
#include "Subscription.h"
#include "Session.h"

namespace DevOpcua {

bool
getYesNo (const char c)
{
    if (strchr("YyTt1", c))
        return true;
    else if (strchr("NnFf0", c))
        return false;
    else
        throw std::runtime_error(SB() << "illegal value '" << c << "'");
}

std::list<std::string>
splitString(const std::string &str, const char delim)
{
    std::list<std::string> tokens;
    size_t prev = 0, sep = 0;
    do {
        sep = str.find_first_of(delim, prev);
        if (sep == std::string::npos)
            sep = str.length();
        std::string token = str.substr(prev, sep - prev);
        // allow escaping delimiters
        while (sep < str.length() && sep > 0 && str[sep - 1] == '\\') {
            prev = sep + 1;
            sep = str.find_first_of(delim, prev);
            if (sep == std::string::npos)
                sep = str.length();
            token.pop_back();
            token.append(str.substr(prev - 1, sep - prev + 1));
        }
        tokens.push_back(token);
        prev = sep + 1;
    } while (sep < str.length() && prev <= str.length());
    return tokens;
}

std::unique_ptr<linkInfo>
parseLink (dbCommon *prec, const DBEntry &ent)
{
    const char *s;
    std::unique_ptr<linkInfo> pinfo (new linkInfo);
    DBLINK *link = ent.getDevLink();
    int debug = prec->tpro;

    if (link->type != INST_IO)
        throw std::logic_error("link is not INST_IO");

    pinfo->isOutput = ent.isOutput();
    pinfo->isItemRecord = ent.isItemRecord();
    pinfo->clientQueueSize = 0;

    if (debug > 4)
        std::cerr << prec->name << " parsing info items" << std::endl;

    // set default from variables and info items
    s = ent.info("opcua:SAMPLING", "");
    if (debug > 19 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:SAMPLING'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->samplingInterval = opcua_DefaultSamplingInterval;
    else {
        std::cerr << prec->name
                  << " DEPRECATION WARNING: setting parameters through info items is deprecated; "
                     "use link parameters instead."
                  << std::endl;
        if (epicsParseDouble(s, &pinfo->samplingInterval, nullptr))
            throw std::runtime_error(SB() << "error converting '" << s << "' to Double");
    }

    s = ent.info("opcua:QSIZE", "");
    if (debug > 19 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:QSIZE'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->queueSize = static_cast<epicsUInt32>(opcua_DefaultServerQueueSize);
    else {
        std::cerr << prec->name
                  << " DEPRECATION WARNING: setting parameters through info items is deprecated; "
                     "use link parameters instead."
                  << std::endl;
        if (epicsParseUInt32(s, &pinfo->queueSize, 0, nullptr))
            throw std::runtime_error(SB() << "error converting '" << s << "' to UInt32");
    }

    s = ent.info("opcua:DISCARD", "");
    if (debug > 19 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:DISCARD'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->discardOldest = !!opcua_DefaultDiscardOldest;
    else {
        std::cerr << prec->name
                  << " DEPRECATION WARNING: setting parameters through info items is deprecated; "
                     "use link parameters instead."
                  << std::endl;
        if (strcmp(s, "new") == 0)
            pinfo->discardOldest = false;
        else if (strcmp(s, "old") == 0)
            pinfo->discardOldest = true;
        else
            throw std::runtime_error(SB() << "illegal value '" << s << "'");
    }

    s = ent.info("opcua:TIMESTAMP", "");
    if (debug > 19 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:TIMESTAMP'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->useServerTimestamp = !!opcua_DefaultUseServerTime;
    else {
        std::cerr << prec->name
                  << " DEPRECATION WARNING: setting parameters through info items is deprecated; "
                     "use link parameters instead."
                  << std::endl;
        if (strcmp(s, "server") == 0)
            pinfo->useServerTimestamp = true;
        else if (strcmp(s, "source") == 0)
            pinfo->useServerTimestamp = false;
        else
            throw std::runtime_error(SB() << "illegal value '" << s << "'");
    }

    s = ent.info("opcua:READBACK", "");
    if (debug > 19 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:READBACK'='" << s << "'" << std::endl;
    if (s[0] == '\0')
        pinfo->monitor = !!opcua_DefaultOutputReadback;
    else {
        std::cerr << prec->name
                  << " DEPRECATION WARNING: setting parameters through info items is deprecated; "
                     "use link parameters instead."
                  << std::endl;
        pinfo->monitor = getYesNo(s[0]);
    }

    s = ent.info("opcua:ELEMENT", "");
    if (debug > 19 && s[0] != '\0')
        std::cerr << prec->name << " info 'opcua:ELEMENT'='" << s << "'" << std::endl;
    if (s[0] != '\0') {
        std::cerr << prec->name
                  << " DEPRECATION WARNING: setting parameters through info items is deprecated; "
                     "use link parameters instead."
                  << std::endl;
        pinfo->element = s;
        pinfo->elementPath = splitString(s);
    }

    // parse INP/OUT link
    if (!link->value.instio.string)
        throw std::runtime_error(SB() << "INP/OUT not set");
    std::string linkstr(link->value.instio.string);
    if (debug > 4)
        std::cerr << prec->name << " parsing inp/out link '" << linkstr << "'" << std::endl;

    size_t sep, send;

    // first token: session or subscription or itemRecord name
    send = linkstr.find_first_of("; \t", 0);
    std::string name = linkstr.substr(0, send);

    Subscription *sub = Subscription::find(name);
    if (sub) {
        pinfo->subscription = name;
        pinfo->session = sub->getSession().getName();
    } else if (Session::find(name)) {
        pinfo->session = name;
    } else if (name != "") {
        DBENTRY entry;
        dbInitEntry(pdbbase, &entry);
        if (dbFindRecord(&entry, name.c_str())) {
            dbFinishEntry(&entry);
            throw std::runtime_error(SB() << "unknown subscription/session/opcuaItemRecord '" << name << "'");
        }
        if (dbFindField(&entry, "RTYP")
                || strcmp(dbGetString(&entry), "opcuaItem")) {
            dbFinishEntry(&entry);
            throw std::runtime_error(SB() << "record '"
                                     << name << "' is not of type opcuaItem");
        }
        pinfo->linkedToItem = false;
        RecordConnector *pconnector = static_cast<RecordConnector *>(static_cast<dbCommon *>(entry.precnode->precord)->dpvt);
        if (!pconnector) {
            dbFinishEntry(&entry);
            throw std::runtime_error(SB() << "opcuaItemRecord '"
                                     << name << "' was not initialized correctly");
        }
        pinfo->item = pconnector->pitem;
        sep = linkstr.find_first_not_of("; \t", send);
        dbFinishEntry(&entry);
    } else {
        throw std::runtime_error(SB() << "link is missing subscription/session/opcuaItemRecord name");
    }

    sep = linkstr.find_first_not_of("; \t", send);

    // everything else is "key=value ..." options
    while (sep != std::string::npos && sep < linkstr.size()) {
        send = linkstr.find_first_of("; \t", sep);
        size_t seq = linkstr.find_first_of('=', sep);

        // allow escaping separators
        while (send != std::string::npos && linkstr[send-1] == '\\') {
                linkstr.erase(send-1, 1);
                send = linkstr.find_first_of("; \t", send);
        }

        if (seq == std::string::npos || (send != std::string::npos && seq >= send))
            throw std::runtime_error(SB() << "expected '=' in '" << linkstr.substr(0, send) << "'");

        std::string optname(linkstr.substr(sep, seq-sep)),
                    optval (linkstr.substr(seq+1, send-seq-1));

        if (debug > 19) {
            std::cerr << prec->name << " opt '" << optname << "'='" << optval << "'" << std::endl;
        }

        // Item/node related options
        if (pinfo->linkedToItem && optname == "ns") {
            if (epicsParseUInt16(optval.c_str(), &pinfo->namespaceIndex, 0, nullptr))
                throw std::runtime_error(SB() << "error converting '" << optval << "' to UInt16");
        } else if (pinfo->linkedToItem && optname == "s") {
            pinfo->identifierString = optval;
            pinfo->identifierIsNumeric = false;
        } else if (pinfo->linkedToItem && optname == "i") {
            if (epicsParseUInt32(optval.c_str(), &pinfo->identifierNumber, 0, nullptr))
                throw std::runtime_error(SB() << "error converting '" << optval << "' to UInt32");
            pinfo->identifierIsNumeric = true;
        } else if (pinfo->linkedToItem && optname == "sampling") {
            if (epicsParseDouble(optval.c_str(), &pinfo->samplingInterval, nullptr))
                throw std::runtime_error(SB() << "error converting '" << optval << "' to Double");
        } else if (pinfo->linkedToItem && optname == "qsize") {
            if (epicsParseUInt32(optval.c_str(), &pinfo->queueSize, 0, nullptr))
                throw std::runtime_error(SB() << "error converting '" << optval << "' to UInt32");
        } else if (pinfo->linkedToItem && optname == "cqsize") {
            if (epicsParseUInt32(optval.c_str(), &pinfo->clientQueueSize, 0, nullptr))
                throw std::runtime_error(SB() << "error converting '" << optval << "' to UInt32");
        } else if (pinfo->linkedToItem && optname == "discard") {
            if (optval == "new")
                pinfo->discardOldest = false;
            else if (optval == "old")
                pinfo->discardOldest = true;
            else
                throw std::runtime_error(SB() << "illegal value '" << optval << "'");
        } else if (pinfo->linkedToItem && optname == "register") {
            if (optval.length() > 0) {
                pinfo->registerNode = getYesNo(optval[0]);
            } else {
                throw std::runtime_error(SB() << "no value for option '" << optname << "'");
            }

        // Item/node or Record/data element related options
        } else if (optname == "timestamp") {
            if (optval == "server")
                pinfo->useServerTimestamp = true;
            else if (optval == "source")
                pinfo->useServerTimestamp = false;
            else
                throw std::runtime_error(SB() << "illegal value '" << optval << "'");
        } else if (optname == "monitor" || optname == "readback") {
            if (optval.length() > 0) {
                pinfo->monitor = getYesNo(optval[0]);
            } else {
                throw std::runtime_error(SB() << "no value for option '" << optname << "'");
            }
        } else if (optname == "element") {
            pinfo->element = optval;
            pinfo->elementPath = splitString(optval);
        } else if (optname == "bini") {
            if (optval == "read")
                pinfo->bini = LinkOptionBini::read;
            else if (optval == "ignore")
                pinfo->bini = LinkOptionBini::ignore;
            else if ((pinfo->isItemRecord || pinfo->isOutput) && optval == "write")
                pinfo->bini = LinkOptionBini::write;
            else
                throw std::runtime_error(SB() << "illegal value '" << optval << "' for option '" << optname << "'");
        } else {
            throw std::runtime_error(SB() << "invalid option '" << optname << "'");
        }

        sep = linkstr.find_first_not_of("; \t", send);
    }

    if (!pinfo->clientQueueSize) {
        pinfo->clientQueueSize = static_cast<epicsUInt32>(ceil(abs(opcua_ClientQueueSizeFactor) * pinfo->queueSize));
        epicsUInt32 mini = static_cast<epicsUInt32>(abs(opcua_MinimumClientQueueSize));
        if (pinfo->clientQueueSize < mini) pinfo->clientQueueSize = mini;
    }

    if (debug > 4) {
        std::cout << prec->name << " :";
        if (pinfo->linkedToItem) {
            if (pinfo->session.length())
                std::cout << " session=" << pinfo->session;
            else if (pinfo->subscription.length())
                std::cout << " subscription=" << pinfo->subscription;
            std::cout << " ns=" << pinfo->namespaceIndex;
            if (pinfo->identifierIsNumeric)
                std::cout << " id(i)=" << pinfo->identifierNumber;
            else
                std::cout << " id(s)=" << pinfo->identifierString;
            std::cout << " sampling=" << pinfo->samplingInterval
                      << " qsize=" << pinfo->queueSize
                      << " cqsize=" << pinfo->clientQueueSize
                      << " discard=" << (pinfo->discardOldest ? "old" : "new")
                      << " registered=" << (pinfo->registerNode ? "y" : "n");
        } else {
            std::cout << " element=" << pinfo->element;
        }
        std::cout << " timestamp=" << (pinfo->useServerTimestamp ? "server" : "source")
                  << " output=" << (pinfo->isOutput ? "y" : "n")
                  << " monitor=" << (pinfo->monitor ? "y" : "n")
                  << " bini=" << linkOptionBiniString(pinfo->bini)
                  << std::endl;
    }

    // consistency checks
    if (pinfo->monitor && pinfo->linkedToItem && !pinfo->subscription.length())
        throw std::runtime_error(SB() << "monitor=y requires link to a subscription");
    if (pinfo->monitor && !pinfo->linkedToItem && !pinfo->item->linkinfo.monitor)
        throw std::runtime_error(SB() << "monitor=y requires link to monitored opcuaItemRecord (but "
                                 << pinfo->item->recConnector->getRecordName() << " is not)");

    return pinfo;
}

} // namespace DevOpcua
