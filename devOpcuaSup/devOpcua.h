/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on code by Michael Davidsaver <mdavidsaver@ospreydcs.com>
 */

#ifndef DEVOPCUA_H
#define DEVOPCUA_H

#include <sstream>
#include <cstring>
#include <memory>

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <dbScan.h>
#include <dbStaticLib.h>
#include <dbAccess.h>

namespace DevOpcua {

class Item;

/** @brief Configuration data for a single record instance.
 *
 * This structure holds all configuration data for a single instance of
 * a supported standard record type, i.e. the result of INP/OUT link parsing.
 *
 * It is kept around, as in the case of a server disconnect/reconnect, the
 * sequence of creating the lower level interface will have to be partially
 * repeated.
 */
typedef struct linkInfo {
    bool linkedToItem = true;
    bool isItemRecord = false;
    Item *item;                        /**< pointer to root item (if structure element) */
    std::string session;
    std::string subscription;

    epicsUInt16 namespaceIndex = 0;
    bool identifierIsNumeric = false;
    epicsUInt32 identifierNumber;
    std::string identifierString;

    bool registerNode = false;

    double samplingInterval;
    epicsUInt32 queueSize;
    bool discardOldest = true;

    std::string element;
    bool useServerTimestamp = true;

    bool isOutput;
    bool monitor = true;
} linkInfo;

/**
 * @brief Enum marking the reason for processing a record
 */
enum ProcessReason { none, incomingData, readComplete, writeComplete };

template<typename R>
struct dset6 {
    long N;
    long (*report)(R*);
    long (*init)(int);
    long (*init_record)(R*);
    long (*get_io_intr_info)(int, dbCommon* prec, IOSCANPVT*);
    long (*readwrite)(R*);
    long (*linconv)(R*);
};

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

struct SB {
    std::ostringstream strm;
    operator std::string() { return strm.str(); }
    template<typename T>
    SB& operator<<(const T& v) {
        strm << v;
        return *this;
    }
};

class DBEntry {
    DBENTRY entry;
public:
    DBENTRY *pentry() const { return const_cast<DBENTRY*>(&entry); }
    explicit DBEntry(dbCommon *prec) {
        dbInitEntry(pdbbase, &entry);
        if (dbFindRecord(&entry, prec->name))
            throw std::logic_error(SB() << "can't find record " << prec->name);
    }
    DBEntry(const DBEntry& ent) {
        dbCopyEntryContents(const_cast<DBENTRY*>(&ent.entry), &entry);
    }
    DBEntry& operator=(const DBEntry& ent) {
        dbFinishEntry(&entry);
        dbCopyEntryContents(const_cast<DBENTRY*>(&ent.entry), &entry);
        return *this;
    }
    ~DBEntry() {
        dbFinishEntry(&entry);
    }
    DBLINK *getDevLink() const {
        if (dbFindField(pentry(), "INP") && dbFindField(pentry(), "OUT"))
            throw std::logic_error(SB() << entry.precnode->recordname << " has no INP/OUT?!");
        if (entry.pflddes->field_type != DBF_INLINK &&
           entry.pflddes->field_type != DBF_OUTLINK)
            throw std::logic_error(SB() << entry.precnode->recordname << " INP/OUT is not a link?!");
        return static_cast<DBLINK*>(entry.pfield);
    }
    bool isOutput() const {
        return !dbFindField(pentry(), "OUT");
    }
    bool isItemRecord() const {
        return !(dbFindField(pentry(), "RTYP") || strcmp(dbGetString(pentry()), "opcuaItem"));
    }
    const char *info(const char *name, const char *def) const
    {
        if (dbFindInfo(pentry(), name))
            return def;
        else
            return entry.pinfonode->string;
    }
};

typedef std::unique_ptr<linkInfo> (*linkParserFunc)(dbCommon*, DBEntry&);

} // namespace DevOpcua

#endif // DEVOPCUA_H
