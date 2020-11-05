/*************************************************************************\
* Copyright (c) 2018-2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 *  and code by Michael Davidsaver <mdavidsaver@ospreydcs.com>
 */

#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>
#include <utility>
#include <cmath>

#include <epicsVersion.h>
#include <epicsStdlib.h>
#include <epicsTypes.h>
#include <epicsTime.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbEvent.h>
#include <iocsh.h>
#include <errlog.h>
#include <dbScan.h>
#include <drvSup.h>
#include <devSup.h>
#include <recSup.h>
#include <recGbl.h>
#include <cvtTable.h>
#include <menuConvert.h>
#include <menuFtype.h>
#include <menuOmsl.h>
#include <alarm.h>

#include <dbCommon.h>
#include <longoutRecord.h>
#include <longinRecord.h>
#ifdef DBR_INT64
#include <int64outRecord.h>
#include <int64inRecord.h>
#endif
#include <mbboDirectRecord.h>
#include <mbbiDirectRecord.h>
#include <mbboRecord.h>
#include <mbbiRecord.h>
#include <boRecord.h>
#include <biRecord.h>
#include <aoRecord.h>
#include <aiRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <lsoRecord.h>
#include <lsiRecord.h>
#include <waveformRecord.h>
#include <aaiRecord.h>
#include <aaoRecord.h>

#include <epicsExport.h>  // defines epicsExportSharedSymbols
#include "opcuaItemRecord.h"
#include "menuDefAction.h"
#include "devOpcua.h"
#include "devOpcuaVersion.h"
#include "RecordConnector.h"
#include "linkParser.h"
#include "ItemUaSdk.h"
#include "DataElementUaSdk.h"

namespace {

using namespace DevOpcua;

#define TRY \
    if (!prec->dpvt) return 0; \
    RecordConnector *pcon = static_cast<RecordConnector*>(prec->dpvt); \
    dbCommon *pdbc = reinterpret_cast<dbCommon*>(prec); \
    try
#define CATCH() catch(std::exception& e) { \
    std::cerr << prec->name << " Error : " << e.what() << std::endl; \
    (void)recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM); \
    return 1; }

// Device Support Extension: link parsing and setup

long
opcua_add_record (dbCommon *prec)
{
    try {
        DBEntry ent(prec);
        std::unique_ptr<RecordConnector> pcon (new RecordConnector(prec));
        ItemUaSdk *pitem;
        pcon->plinkinfo = parseLink(prec, ent);
        if (prec->pini) reportPiniAndClear(prec);
        //TODO: Switch to implementation selection / factory
        if (pcon->plinkinfo->linkedToItem) {
            pitem = new ItemUaSdk(*pcon->plinkinfo);
            pitem->recConnector = pcon.get();
        } else {
            pitem = static_cast<ItemUaSdk *>(pcon->plinkinfo->item);
        }
        DataElementUaSdk::addElementChain(pitem, pcon.get(), pcon->plinkinfo->element);
        pcon->pitem = pitem;
        prec->dpvt = pcon.release();
        return 0;
    } catch(std::exception& e) {
        std::cerr << prec->name << " Error in add_record : " << e.what() << std::endl;
        return S_dbLib_badLink;
    }
}

long
opcua_del_record (dbCommon *prec)
{
    //TODO: support changing OPCUA links
    return -1;
}

dsxt opcua_dsxt = { opcua_add_record, opcua_del_record };

// Initialization

long
opcua_init (int pass)
{
    static bool blurp = false;

    if (!blurp) {
        errlogPrintf("OPC UA Client Device Support %u.%u.%u%s (%s); using %s\n",
                     EPICS_OPCUA_MAJOR_VERSION,
                     EPICS_OPCUA_MINOR_VERSION,
                     EPICS_OPCUA_MAINTENANCE_VERSION,
                     (EPICS_OPCUA_DEVELOPMENT_FLAG ? "-dev" : ""),
                     EPICS_OPCUA_GIT_COMMIT,
                     opcuaGetDriverName().c_str());
        blurp = true;
    }

    if (pass == 0) devExtend(&opcua_dsxt);
    return 0;
}

template<typename REC>
long
opcua_init_mask_read (REC* prec) {
    if (prec->nobt == 0) prec->mask = 0xffffffff;
    prec->mask <<= prec->shft;
    return 0;
}

template<typename REC>
long
opcua_init_mask_write (REC* prec) {
    if (prec->nobt == 0) prec->mask = 0xffffffff;
    prec->mask <<= prec->shft;
    return 2;
}

// Get I/O interrupt information

long
opcua_get_ioint (int cmd, dbCommon *prec, IOSCANPVT *ppvt)
{
    if (!prec->dpvt) return 0;
    RecordConnector *pcon = static_cast<RecordConnector *>(prec->dpvt);
    pcon->isIoIntrScanned = (cmd ? false : true);
    *ppvt = pcon->ioscanpvt;
    return 0;
}

// helper: decide if read value should be used

inline bool
useReadValue (RecordConnector *pcon) {
    if (pcon->reason == ProcessReason::incomingData ||
            pcon->reason == ProcessReason::none ||
            (pcon->reason == ProcessReason::readComplete &&
             (pcon->state() != ConnectionStatus::initialRead ||
              pcon->bini() == LinkOptionBini::read)))
        return true;
    else
        return false;
}

// helper: manage status and bini behavior

inline void
manageStateAndBiniProcessing (RecordConnector *pcon) {
    if (pcon->state() == ConnectionStatus::initialRead &&
            pcon->reason == ProcessReason::readComplete) {
        if (pcon->bini() == LinkOptionBini::write) {
            if (pcon->plinkinfo->linkedToItem) {
                pcon->setState(ConnectionStatus::initialWrite);
            }
            pcon->requestRecordProcessing(ProcessReason::writeRequest);
        } else {
            pcon->setState(ConnectionStatus::up);
        }
    }
}

// helper: trace printout for read and write

enum ItemAction { none, read, write };

inline const char *
itemActionString (const ItemAction choice)
{
    switch(choice) {
    case none:   return "no";
    case read:   return "read";
    case write:  return "write";
    }
    return "???";
}

inline void
traceWritePrint (const dbCommon *pdbc, const RecordConnector *pcon, const epicsInt32 value, const char*field = "VAL") {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: write (state=%s, reason=%s) <- %s=%d (%#010x)\n",
                     pdbc->name,
                     connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     field,
                     value, static_cast<unsigned int>(value));
}

inline void
traceWritePrint (const dbCommon *pdbc, const RecordConnector *pcon, const epicsUInt32 value, const char *field = "VAL") {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: write (state=%s, reason=%s) <- %s=%u (%#010x)\n",
                     pdbc->name,
                     connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     field,
                     value, value);
}

inline void
traceWritePrint (const dbCommon *pdbc, const RecordConnector *pcon, const epicsInt64 value, const char *field = "VAL") {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: write (state=%s, reason=%s) <- %s=%lld (%#018llx)\n",
                     pdbc->name,
                     connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     field,
                     value, static_cast<unsigned long long int>(value));
}

inline void
traceWritePrint (const dbCommon *pdbc, const RecordConnector *pcon, const double value) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: write (state=%s, reason=%s) <- VAL=%g\n",
                     pdbc->name,
                     connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     value);
}

inline void
traceWritePrint (const dbCommon *pdbc, const RecordConnector *pcon, const char *value) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: write (state=%s, reason=%s) <- VAL='%s'\n",
                     pdbc->name,
                     connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     value);
}

inline void
traceWriteArrayPrint (const dbCommon *pdbc, const RecordConnector *pcon, const epicsUInt32 size) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: write (state=%s, reason=%s) <- %d array elements written\n",
                     pdbc->name,
                     connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     size);
}

inline void
traceReadPrint (const dbCommon *pdbc, const RecordConnector *pcon,
                const long ret, const bool setVal, const epicsInt32 value, const char *field = "VAL") {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: read (state=%s, reason=%s, status=%ld, setVal=%c) -> %s=%d (%#010x)\n",
                     pdbc->name, connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     ret, setVal ? 'y' : 'n',
                     field,
                     value, static_cast<unsigned int>(value));
}

inline void
traceReadPrint (const dbCommon *pdbc, const RecordConnector *pcon,
                const long ret, const bool setVal, const epicsUInt32 value, const char *field = "VAL") {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: read (state=%s, reason=%s, status=%ld, setVal=%c) -> %s=%u (%#010x)\n",
                     pdbc->name, connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     ret, setVal ? 'y' : 'n',
                     field,
                     value, value);
}

inline void
traceReadPrint (const dbCommon *pdbc, const RecordConnector *pcon,
                const long ret, const bool setVal, const epicsInt64 value) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: read (state=%s, reason=%s, status=%ld, setVal=%c) -> VAL=%lld (%#018llx)\n",
                     pdbc->name, connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     ret, setVal ? 'y' : 'n',
                     value, static_cast<unsigned long long int>(value));
}

inline void
traceReadPrint (const dbCommon *pdbc, const RecordConnector *pcon,
                const long ret, const bool setVal, const double value) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: read (state=%s, reason=%s, status=%ld, setVal=%c) -> VAL=%g\n",
                     pdbc->name, connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     ret, setVal ? 'y' : 'n',
                     value);
}

inline void
traceReadPrint (const dbCommon *pdbc, const RecordConnector *pcon,
                const long ret, const bool setVal, const double value, const epicsInt32 rvalue) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: read (state=%s, reason=%s, status=%ld, setVal=%c) -> VAL=%g converted from RVAL=%d (%#010x)\n",
                     pdbc->name, connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     ret, setVal ? 'y' : 'n',
                     value, rvalue, static_cast<unsigned int>(rvalue));
}

inline void
traceReadPrint (const dbCommon *pdbc, const RecordConnector *pcon,
                const long ret, const bool setVal, const epicsUInt16 value, const epicsUInt32 rvalue) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: read (state=%s, reason=%s, status=%ld, setVal=%c) -> VAL=%u converted from RVAL=%u (%#010x)\n",
                     pdbc->name, connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     ret, setVal ? 'y' : 'n',
                     value, rvalue, static_cast<unsigned int>(rvalue));
}

inline void
traceReadPrint (const dbCommon *pdbc, const RecordConnector *pcon,
                const long ret, const bool setVal, const char *value) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: read (state=%s, reason=%s, status=%ld, setVal=%c) -> VAL='%s'\n",
                     pdbc->name, connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     ret, setVal ? 'y' : 'n',
                     value);
}

inline void
traceReadArrayPrint (const dbCommon *pdbc, const RecordConnector *pcon,
                     const long ret, const bool setVal, const epicsUInt32 size) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: read (state=%s, reason=%s, status=%ld, setVal=%c) -> %d array elements read\n",
                     pdbc->name, connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     ret, setVal ? 'y' : 'n',
                     size);
}

inline void
traceItemActionPrint (const dbCommon *pdbc, const RecordConnector *pcon,
                      const long ret, const ItemAction action, const epicsUInt32 code, const char *text) {
    if (pdbc->tpro > 1)
        errlogPrintf("%s: processed (state=%s, reason=%s, status=%ld) -> %s request sent; status code %#10x (%s)\n",
                     pdbc->name, connectionStatusString(pcon->state()),
                     processReasonString(pcon->reason),
                     ret, itemActionString(action), code, text);
}

// integer to/from VAL

template<typename REC>
long
opcua_read_int32_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pcon->requestOpcuaRead();
        } else {
            epicsInt32 *value = useReadValue(pcon) ? &prec->val : nullptr;
            ret = pcon->readScalar(value, &nextReason);
            traceReadPrint(pdbc, pcon, ret, value, prec->val);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

template<typename REC>
long
opcua_write_int32_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                traceWritePrint(pdbc, pcon, prec->val);
                pcon->writeScalar(prec->val);
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            epicsInt32 *value = useReadValue(pcon) ? &prec->val : nullptr;
            ret = pcon->readScalar(value, &nextReason);
            traceReadPrint(pdbc, pcon, ret, value, prec->val);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

template<typename REC>
long
opcua_read_int64_val (REC *prec)
{
    long ret =0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pcon->requestOpcuaRead();
        } else {
            epicsInt64 *value = useReadValue(pcon) ? &prec->val : nullptr;
            ret = pcon->readScalar(value, &nextReason);
            traceReadPrint(pdbc, pcon, ret, value, prec->val);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

template<typename REC>
long
opcua_write_int64_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                traceWritePrint(pdbc, pcon, prec->val);
                pcon->writeScalar(prec->val);
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            epicsInt64 *value = useReadValue(pcon) ? &prec->val : nullptr;
            ret = pcon->readScalar(value, &nextReason);
            traceReadPrint(pdbc, pcon, ret, value, prec->val);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

// unsigned integer to/from RVAL

template<typename REC>
long
opcua_read_uint32_rval (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pcon->requestOpcuaRead();
        } else {
            epicsUInt32 *value = useReadValue(pcon) ? &prec->rval : nullptr;
            ret = pcon->readScalar(value, &nextReason);
            traceReadPrint(pdbc, pcon, ret, value, prec->rval, "RVAL");
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

template<typename REC>
long
opcua_write_uint32_rval (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                traceWritePrint(pdbc, pcon, prec->rval, "RVAL");
                pcon->writeScalar(prec->rval);
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            epicsUInt32 *value = useReadValue(pcon) ? &prec->rval : nullptr;
            ret = pcon->readScalar(value, &nextReason);
            traceReadPrint(pdbc, pcon, ret, value, prec->rval, "RVAL");
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

// analog input/output

template<typename REC>
long
opcua_read_analog (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pcon->requestOpcuaRead();
        } else {
            bool setValFromValue = useReadValue(pcon);
            if (prec->linr == menuConvertNO_CONVERSION) {
                double value = 0.0;
                ret = pcon->readScalar(&value, &nextReason);
                if (ret == 0) {
                    if (setValFromValue) {
                        // Do ASLO/AOFF conversion and smoothing
                        if (prec->aslo != 0.0) value *= prec->aslo;
                        value += prec->aoff;
                        if (prec->smoo == 0.0 || prec->udf || !std::isfinite(prec->val))
                            prec->val = value;
                        else
                            prec->val = prec->val * prec->smoo + value * (1.0 - prec->smoo);
                        prec->udf = 0;
                    }
                    ret = 2;     // don't convert
                }
                traceReadPrint(pdbc, pcon, ret, setValFromValue, prec->val);
            } else {
                if (setValFromValue) {
                    ret = pcon->readScalar(&prec->rval, &nextReason);
                } else {
                    ret = 2;     // don't convert
                }
                traceReadPrint(pdbc, pcon, ret, setValFromValue, prec->rval, "RVAL");
            }
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

template<typename REC>
long
opcua_write_analog (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        //TODO: ignore incoming data when output rate limit active
        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                if (prec->linr == menuConvertNO_CONVERSION) {
                    traceWritePrint(pdbc, pcon, prec->val);
                    ret = pcon->writeScalar(prec->val);
                } else {
                    traceWritePrint(pdbc, pcon, prec->rval, "RVAL");
                    ret = pcon->writeScalar(prec->rval);
                }
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            double value = 0.0;
            bool setValFromValue = useReadValue(pcon);

            if (prec->linr == menuConvertNO_CONVERSION) {
                ret = pcon->readScalar(&value, &nextReason);
                if (ret == 0) {
                    if (prec->aslo != 0.0) value *= prec->aslo;
                    value += prec->aoff;
                }
            } else {
                epicsInt32 rvalue = 0;
                ret = pcon->readScalar(&rvalue, &nextReason);
                if (ret == 0) {
                    value = static_cast<double>(rvalue)
                            + static_cast<double>(prec->roff);
                    if (prec->aslo != 0.0) value *= prec->aslo;
                    value += prec->aoff;
                    if (prec->linr == menuConvertLINEAR || prec->linr == menuConvertSLOPE) {
                        value = value * prec->eslo + prec->eoff;
                    } else {
                        if (cvtRawToEngBpt(&value, static_cast<short>(prec->linr), prec->init,
                                           static_cast<void **>(&prec->pbrk), &prec->lbrk) != 0)
                            setValFromValue = false;
                    }
                    if (setValFromValue)
                        prec->rval = rvalue;
                }
            }
            if (setValFromValue) {
                prec->val = value;
                prec->udf = std::isnan(prec->val);
            }

            if (prec->linr == menuConvertNO_CONVERSION)
                traceReadPrint(pdbc, pcon, ret, setValFromValue, prec->val);
            else
                traceReadPrint(pdbc, pcon, ret, setValFromValue, prec->val, prec->rval);

            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

// enum output

template<typename REC>
long
opcua_write_enum (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                traceWritePrint(pdbc, pcon, prec->rval, "RVAL");
                pcon->writeScalar(prec->rval);
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            epicsUInt32 *rval = useReadValue(pcon) ? &prec->rval : nullptr;
            ret = pcon->readScalar(rval, &nextReason);
            if (ret == 0 && rval) {
                *rval &= prec->mask;
                if (prec->shft)
                    *rval >>= prec->shft;
                if (prec->sdef) {
                    epicsUInt32 *pstate_values = &prec->zrvl;
                    prec->val = 65535;   // unknown state
                    for (epicsUInt16 i = 0; i < 16; i++) {
                        if (*pstate_values == *rval) {
                            prec->val = i;
                            break;
                        }
                        pstate_values++;
                    }
                } else {
                    // no defined states
                    prec->val = static_cast<epicsUInt16>(*rval);
                }
            }
            traceReadPrint(pdbc, pcon, ret, rval, prec->val, prec->rval);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

// bo output

template<typename REC>
long
opcua_write_bo (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                traceWritePrint(pdbc, pcon, prec->rval, "RVAL");
                pcon->writeScalar(prec->rval);
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            epicsUInt32 *rval = useReadValue(pcon) ? &prec->rval : nullptr;
            ret = pcon->readScalar(rval, &nextReason);
            if (ret == 0 && rval) {
                if (*rval == 0) prec->val = 0;
                else prec->val = 1;
            }
            traceReadPrint(pdbc, pcon, ret, rval, prec->val, prec->rval);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

// mbboDirect output

template<typename REC>
long
opcua_write_mbbod (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                traceWritePrint(pdbc, pcon, prec->rval, "RVAL");
                pcon->writeScalar(prec->rval);
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            epicsUInt32 *rval = useReadValue(pcon) ? &prec->rval : nullptr;
            ret = pcon->readScalar(rval, &nextReason);
            if (ret == 0 && rval) {
                if (prec->shft > 0)
                    *rval >>= prec->shft;
                prec->val = *rval;
                prec->udf = FALSE;
                if (!prec->udf &&
                        prec->omsl == menuOmslsupervisory) {
                    /* Set B0 - Bnn from VAL */
                    epicsUInt32 val = prec->val;
                    epicsUInt8 *pBn = &prec->b0;
                    for (int i = 0; i < static_cast<int>(8 * sizeof(prec->val)); i++) {
                        *pBn++ = !! (val & 1);
                        val >>= 1;
                    }
                }
            }
            traceReadPrint(pdbc, pcon, ret, rval, prec->val, prec->rval);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

// string to/from VAL

template<typename REC>
long
opcua_read_string_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pcon->requestOpcuaRead();
        } else {
            char *value = useReadValue(pcon) ? prec->val : nullptr;
            unsigned short num = value ? MAX_STRING_SIZE : 0;
            ret = pcon->readScalar(value, num, &nextReason);
            traceReadPrint(pdbc, pcon, ret, value, prec->val);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

template<typename REC>
long
opcua_write_string_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                traceWritePrint(pdbc, pcon, prec->val);
                ret = pcon->writeScalar(prec->val, MAX_STRING_SIZE);
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            char *value = useReadValue(pcon) ? prec->val : nullptr;
            unsigned short num = value ? MAX_STRING_SIZE : 0;
            ret = pcon->readScalar(value, num, &nextReason);
            traceReadPrint(pdbc, pcon, ret, value, prec->val);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

// long string to/from VAL

template<typename REC>
long
opcua_read_lstring_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pcon->requestOpcuaRead();
        } else {
            char *value = useReadValue(pcon) ? prec->val : nullptr;
            ret = pcon->readScalar(value, prec->sizv, &nextReason);
            if (value)
                prec->len = static_cast<epicsUInt32>(strlen(prec->val) + 1);
            traceReadPrint(pdbc, pcon, ret, value, prec->val);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

template<typename REC>
long
opcua_write_lstring_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                traceWritePrint(pdbc, pcon, prec->val);
                ret = pcon->writeScalar(prec->val, prec->sizv);
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            char *value = useReadValue(pcon) ? prec->val : nullptr;
            ret = pcon->readScalar(value, prec->sizv, &nextReason);
            if (value)
                prec->len = static_cast<epicsUInt32>(strlen(prec->val) + 1);
            traceReadPrint(pdbc, pcon, ret, value, prec->val);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

// array to/from VAL

template<typename REC>
long
opcua_read_array (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;
        epicsUInt32 nord = prec->nord;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pcon->requestOpcuaRead();
        } else {
            void *bptr = useReadValue(pcon) ? prec->bptr : nullptr;
            switch (prec->ftvl) {
            case menuFtypeSTRING:
                ret = pcon->readArray(static_cast<char *>(bptr), MAX_STRING_SIZE, prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeCHAR:
                ret = pcon->readArray(static_cast<epicsInt8 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUCHAR:
                ret = pcon->readArray(static_cast<epicsUInt8 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeSHORT:
                ret = pcon->readArray(static_cast<epicsInt16 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUSHORT:
                ret = pcon->readArray(static_cast<epicsUInt16 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeLONG:
                ret = pcon->readArray(static_cast<epicsInt32 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeULONG:
                ret = pcon->readArray(static_cast<epicsUInt32 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
#ifdef DBR_INT64
            case menuFtypeINT64:
                ret = pcon->readArray(static_cast<epicsInt64 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUINT64:
                ret = pcon->readArray(static_cast<epicsUInt64 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
#endif
            case menuFtypeFLOAT:
                ret = pcon->readArray(static_cast<epicsFloat32 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeDOUBLE:
                ret = pcon->readArray(static_cast<epicsFloat64 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeENUM:
                ret = pcon->readArray(static_cast<epicsUInt16 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            }
            if (nord != prec->nord)
                db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);            
            traceReadArrayPrint(pdbc, pcon, ret, bptr, prec->nord);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

template<typename REC>
long
opcua_write_array (REC *prec)
{    
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ProcessReason nextReason = ProcessReason::none;
        epicsUInt32 nord = prec->nord;

        if (pcon->reason == ProcessReason::none || pcon->reason == ProcessReason::writeRequest) {
            if (!(pcon->state() == ConnectionStatus::down || pcon->state() == ConnectionStatus::initialRead)) {
                traceWriteArrayPrint(pdbc, pcon, prec->nord);
                switch (prec->ftvl) {
                case menuFtypeSTRING:
                    ret = pcon->writeArray(static_cast<char *>(prec->bptr), MAX_STRING_SIZE, prec->nord);
                    break;
                case menuFtypeCHAR:
                    ret = pcon->writeArray(static_cast<epicsInt8 *>(prec->bptr), prec->nord);
                    break;
                case menuFtypeUCHAR:
                    ret = pcon->writeArray(static_cast<epicsUInt8 *>(prec->bptr), prec->nord);
                    break;
                case menuFtypeSHORT:
                    ret = pcon->writeArray(static_cast<epicsInt16 *>(prec->bptr), prec->nord);
                    break;
                case menuFtypeUSHORT:
                    ret = pcon->writeArray(static_cast<epicsUInt16 *>(prec->bptr), prec->nord);
                    break;
                case menuFtypeLONG:
                    ret = pcon->writeArray(static_cast<epicsInt32 *>(prec->bptr), prec->nord);
                    break;
                case menuFtypeULONG:
                    ret = pcon->writeArray(static_cast<epicsUInt32 *>(prec->bptr), prec->nord);
                    break;
#ifdef DBR_INT64
                case menuFtypeINT64:
                    ret = pcon->writeArray(static_cast<epicsInt64 *>(prec->bptr), prec->nord);
                    break;
                case menuFtypeUINT64:
                    ret = pcon->writeArray(static_cast<epicsUInt64 *>(prec->bptr), prec->nord);
                    break;
#endif
                case menuFtypeFLOAT:
                    ret = pcon->writeArray(static_cast<epicsFloat32 *>(prec->bptr), prec->nord);
                    break;
                case menuFtypeDOUBLE:
                    ret = pcon->writeArray(static_cast<epicsFloat64 *>(prec->bptr), prec->nord);
                    break;
                case menuFtypeENUM:
                    ret = pcon->writeArray(static_cast<epicsUInt16 *>(prec->bptr), prec->nord);
                    break;
                }
                if (pcon->plinkinfo->linkedToItem &&
                        (pcon->state() == ConnectionStatus::up ||
                         pcon->state() == ConnectionStatus::initialWrite)) {
                    prec->pact = true;
                    pcon->requestOpcuaWrite();
                }
            }
        } else {
            void *bptr = useReadValue(pcon) ? prec->bptr : nullptr;
            switch (prec->ftvl) {
            case menuFtypeSTRING:
                ret = pcon->readArray(static_cast<char *>(bptr), MAX_STRING_SIZE, prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeCHAR:
                ret = pcon->readArray(static_cast<epicsInt8 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUCHAR:
                ret = pcon->readArray(static_cast<epicsUInt8 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeSHORT:
                ret = pcon->readArray(static_cast<epicsInt16 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUSHORT:
                ret = pcon->readArray(static_cast<epicsUInt16 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeLONG:
                ret = pcon->readArray(static_cast<epicsInt32 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeULONG:
                ret = pcon->readArray(static_cast<epicsUInt32 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
#ifdef DBR_INT64
            case menuFtypeINT64:
                ret = pcon->readArray(static_cast<epicsInt64 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUINT64:
                ret = pcon->readArray(static_cast<epicsUInt64 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
#endif
            case menuFtypeFLOAT:
                ret = pcon->readArray(static_cast<epicsFloat32 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeDOUBLE:
                ret = pcon->readArray(static_cast<epicsFloat64 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeENUM:
                ret = pcon->readArray(static_cast<epicsUInt16 *>(bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            }
            if (nord != prec->nord)
                db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);

            traceReadArrayPrint(pdbc, pcon, ret, bptr, prec->nord);
            manageStateAndBiniProcessing(pcon);
        }
        if (nextReason != ProcessReason::none)
            pcon->requestRecordProcessing(nextReason);
    } CATCH()
    return ret;
}

// opcuaItemRecord

template<typename REC>
long
opcua_action_item (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pcon->lock);
        ItemAction action = none;

        switch (pcon->reason) {
        case ProcessReason::readFailure:
            (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            ret = 1;
            break;
        case ProcessReason::writeFailure:
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
            break;
        case ProcessReason::connectionLoss:
            (void) recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM);
            ret = 1;
            break;
        case ProcessReason::readRequest:
            prec->pact = true;
            action = read;
            pcon->requestOpcuaRead();
            break;
        case ProcessReason::writeRequest:
            prec->pact = true;
            action= write;
            pcon->requestOpcuaWrite();
            break;
        case ProcessReason::none:
            prec->pact = true;
            if (prec->defactn == menuDefActionREAD) {
                action = read;
                pcon->requestOpcuaRead();
            } else {
                action= write;
                pcon->requestOpcuaWrite();
            }
            break;
        default:
            break;
        }
        pcon->getStatus(&prec->statcode, prec->stattext, MAX_STRING_SIZE+1);
        traceItemActionPrint(pdbc, pcon, ret, action, prec->statcode, prec->stattext);
    } CATCH()
    return ret;
}

} // namespace

#define SUP(NAME, REC, OP, DIR) static dset6<REC##Record> NAME = \
{6, nullptr, opcua_init, nullptr, opcua_get_ioint, &opcua_##DIR##_##OP<REC##Record>, nullptr}; \
    extern "C" { epicsExportAddress(dset, NAME); }

#define SUPM(NAME, REC, OP, DIR) static dset6<REC##Record> NAME = \
{6, nullptr, opcua_init, opcua_init_mask_##DIR<REC##Record>, opcua_get_ioint, &opcua_##DIR##_##OP<REC##Record>, nullptr}; \
    extern "C" { epicsExportAddress(dset, NAME); }

#define SUPI(NAME, REC, OP, DIR) static dset6<REC##Record> NAME = \
{6, nullptr, nullptr, nullptr, opcua_get_ioint, &opcua_##DIR##_##OP<REC##Record>, nullptr}; \
    extern "C" { epicsExportAddress(dset, NAME); }

SUP (devLiOpcua,             longin,   int32_val, read)
SUP (devLoOpcua,            longout,   int32_val, write)
SUP (devBiOpcua,                 bi, uint32_rval, read)
SUP (devBoOpcua,                 bo,          bo, write)
SUPM(devMbbiOpcua,             mbbi, uint32_rval, read)
SUPM(devMbboOpcua,             mbbo,        enum, write)
SUPM(devMbbiDirectOpcua, mbbiDirect, uint32_rval, read)
SUPM(devMbboDirectOpcua, mbboDirect,       mbbod, write)
SUP (devAiOpcua,                 ai,      analog, read)
SUP (devAoOpcua,                 ao,      analog, write)
SUP (devSiOpcua,           stringin,  string_val, read)
SUP (devSoOpcua,          stringout,  string_val, write)
SUP (devLsiOpcua,               lsi, lstring_val, read)
SUP (devLsoOpcua,               lso, lstring_val, write)
SUP (devWfOpcua,           waveform,       array, read)
SUP (devAaiOpcua,               aai,       array, read)
SUP (devAaoOpcua,               aao,       array, write)
#ifdef DBR_INT64
SUP (devInt64inOpcua,       int64in,   int64_val, read)
SUP (devInt64outOpcua,     int64out,   int64_val, write)
#endif
SUPI(devItemOpcua,        opcuaItem,        item, action)
