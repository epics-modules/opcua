/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 *  and example code from the Unified Automation C++ Based OPC UA Client SDK
 */

#ifndef DEVOPCUA_H
#define DEVOPCUA_H

#include <sstream>

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <dbScan.h>
#include <dbStaticLib.h>
#include <dbAccess.h>

template<typename R>
struct dset6 {
    long N;
    long (*report)(R*);
    long (*init)(int);
    long (*init_record)(dbCommon*);
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
            throw std::logic_error(SB() << "getLink can't find record " << prec->name);
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
            throw std::logic_error(SB() << entry.precnode->recordname << " has no INP/OUT?!?!");
        if (entry.pflddes->field_type != DBF_INLINK &&
           entry.pflddes->field_type != DBF_OUTLINK)
            throw std::logic_error(SB() << entry.precnode->recordname << " not devlink or IN/OUT?!?!");
        return (DBLINK*)entry.pfield;
    }
    const char *info(const char *name, const char *def) const
    {
        if (dbFindInfo(pentry(), name))
            return def;
        else
            return entry.pinfonode->string;
    }
};

#endif // DEVOPCUA_H
