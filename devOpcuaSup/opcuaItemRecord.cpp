/*************************************************************************\
* Copyright (c) 2018-2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * OPC UA Item record type
 *
 * Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <iostream>

#include <dbDefs.h>
#include <errlog.h>
#include <alarm.h>
#include <cantProceed.h>
#include <dbAccess.h>
#include <dbEvent.h>
#include <dbFldTypes.h>
#include <errMdef.h>
#include <menuPost.h>
#include <menuYesNo.h>
#include <devSup.h>
#include <recSup.h>
#include <recGbl.h>
#include <special.h>

#include <epicsExport.h>  // defines epicsExportSharedSymbols
#define GEN_SIZE_OFFSET
#include "opcuaItemRecord.h"
#undef GEN_SIZE_OFFSET
#include "devOpcua.h"
#include "ItemUaSdk.h"    //FIXME: replace item creation with factory call (see below)
#include "RecordConnector.h"
#include "linkParser.h"

#include <epicsVersion.h>
#ifdef VERSION_INT
#  if EPICS_VERSION_INT < VERSION_INT(3,16,0,2)
#    define RECSUPFUN_CAST(F) reinterpret_cast<RECSUPFUN>(F)
#  else
#    define RECSUPFUN_CAST(F) F
#  endif
#else
#  define RECSUPFUN_CAST reinterpret_cast<RECSUPFUN>(F)
#endif

namespace {

using namespace DevOpcua;

long readwrite(opcuaItemRecord *prec);
void monitor(opcuaItemRecord *);

long
init_record (dbCommon *pdbc, int pass)
{
    opcuaItemRecord *prec = reinterpret_cast<opcuaItemRecord *>(pdbc);

    if (pass == 0) {
        try {
            DBEntry ent(pdbc);
            std::unique_ptr<RecordConnector> pvt (new RecordConnector(pdbc));
            pvt->plinkinfo = parseLink(pdbc, ent);
            if (pdbc->pini) reportPiniAndClear(pdbc);
            ItemUaSdk *pitem = new ItemUaSdk(*pvt->plinkinfo); //FIXME: replace item creation with factory call
            pitem->recConnector = pvt.get();
            pvt->pitem = pitem;
            prec->dpvt = pvt.release();
            strncpy(prec->sess, pitem->linkinfo.session.c_str(), MAX_STRING_SIZE);
            prec->sess[MAX_STRING_SIZE] = '\0';
            strncpy(prec->subs, pitem->linkinfo.subscription.c_str(), MAX_STRING_SIZE);
            prec->subs[MAX_STRING_SIZE] = '\0';
        } catch(std::exception& e) {
            std::cerr << prec->name << " Error in init_record : " << e.what() << std::endl;
            return S_dbLib_badLink;
        }
    }

    return 0;
}

long
process (dbCommon *pdbc)
{
    auto prec  = reinterpret_cast<opcuaItemRecord *>(pdbc);
    auto pdset = reinterpret_cast<struct dset6<opcuaItemRecord> *>(prec->dset);
    auto preason = &(reinterpret_cast<RecordConnector *>(prec->dpvt)->reason);

    int pact = prec->pact;
    long status = 0;

    if( (pdset == nullptr) || (pdset->readwrite == nullptr) ) {
        prec->pact = true;
        recGblRecordError(S_dev_missingSup, static_cast<void *>(prec), "readwrite");
        return S_dev_missingSup;
    }

    status = readwrite(prec);
    *preason = ProcessReason::none;

    if (!pact && prec->pact)
        return 0;

    prec->pact = true;
    recGblGetTimeStamp(prec);

    monitor(prec);

    /* Wrap up */
    recGblFwdLink(prec);
    prec->pact = false;
    return status;
}

long
special (DBADDR *paddr, int after)
{
    if (!after)
        return 0;

    if (paddr->special == SPC_MOD) {
        int fieldIndex = dbGetFieldIndex(paddr);
        auto preason = &(reinterpret_cast<RecordConnector *>(paddr->precord->dpvt)->reason);
        if (fieldIndex == opcuaItemRecordWRITE) {
            *preason = ProcessReason::writeRequest;
        } else if (fieldIndex == opcuaItemRecordREAD) {
            *preason = ProcessReason::readRequest;
        }
    }

    return 0;
}

void
monitor (opcuaItemRecord *prec)
{
    epicsUInt16 events = recGblResetAlarms(prec);

    if (prec->ostatcode != prec->statcode) {
        db_post_events(prec, &prec->statcode, events|DBE_VALUE|DBE_LOG);
        db_post_events(prec, &prec->stattext, events|DBE_VALUE|DBE_LOG);
        prec->ostatcode = prec->statcode;
    }
}

long
readwrite (opcuaItemRecord *prec)
{
    auto pdset = reinterpret_cast<struct dset6<opcuaItemRecord> *>(prec->dset);
    long status = 0;

    status = pdset->readwrite(prec);

    if (!status)
        prec->udf = FALSE;

    return status;
}

/* Create Record Support Entry Table */

#define report nullptr
#define initialize nullptr
/* init_record */
/* process */
/* special */
#define get_value nullptr
#define cvt_dbaddr nullptr
#define get_array_info nullptr
#define put_array_info nullptr
#define get_units nullptr
#define get_precision nullptr
#define get_enum_str nullptr
#define get_enum_strs nullptr
#define put_enum_str nullptr
#define get_graphic_double nullptr
#define get_control_double nullptr
#define get_alarm_double nullptr

rset opcuaItemRSET = {
    RSETNUMBER,
    report,
    initialize,
    RECSUPFUN_CAST(init_record),
    RECSUPFUN_CAST(process),
    RECSUPFUN_CAST(special),
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double
};
extern "C" { epicsExportAddress(rset, opcuaItemRSET); }

} // namespace
