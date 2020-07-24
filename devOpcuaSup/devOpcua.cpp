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
    RecordConnector *pvt = static_cast<RecordConnector*>(prec->dpvt); \
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
        std::unique_ptr<RecordConnector> pvt (new RecordConnector(prec));
        ItemUaSdk *pitem;
        pvt->plinkinfo = parseLink(prec, ent);
        //TODO: Switch to implementation selection / factory
        if (pvt->plinkinfo->linkedToItem) {
            pitem = new ItemUaSdk(*pvt->plinkinfo);
        } else {
            pitem = static_cast<ItemUaSdk *>(pvt->plinkinfo->item);
        }
        DataElementUaSdk::addElementChain(pitem, pvt.get(), pvt->plinkinfo->element);
        pvt->pitem = pitem;
        prec->dpvt = pvt.release();
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
    RecordConnector *pvt = static_cast<RecordConnector *>(prec->dpvt);
    pvt->isIoIntrScanned = (cmd ? false : true);
    *ppvt = pvt->ioscanpvt;
    return 0;
}

// integer to/from VAL

template<typename REC>
long
opcua_read_int32_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pvt->requestOpcuaRead();
        } else {
            ret = pvt->readScalar(&prec->val, &nextReason);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL=%d (%#010x)\n",
                             prec->name, ret, prec->val,
                             static_cast<unsigned int>(prec->val));
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

template<typename REC>
long
opcua_write_int32_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->tpro > 1)
                errlogPrintf("%s: write <- VAL=%d (%#010x)\n",
                             prec->name, prec->val,
                             static_cast<unsigned int>(prec->val));
            pvt->writeScalar(prec->val);
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            ret = pvt->readScalar(&prec->val, &nextReason);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL=%d (%#010x)\n",
                             prec->name, ret, prec->val,
                             static_cast<unsigned int>(prec->val));
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

template<typename REC>
long
opcua_read_int64_val (REC *prec)
{
    long ret =0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pvt->requestOpcuaRead();
        } else {
            ret = pvt->readScalar(&prec->val, &nextReason);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL=%lld (%#018llx)\n",
                             prec->name, ret, prec->val,
                             static_cast<unsigned long long int>(prec->val));
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

template<typename REC>
long
opcua_write_int64_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->tpro > 1)
                errlogPrintf("%s: write <- VAL=%lld (%#018llx)\n",
                             prec->name, prec->val,
                             static_cast<unsigned long long int>(prec->val));
            pvt->writeScalar(prec->val);
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            ret = pvt->readScalar(&prec->val, &nextReason);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL=%lld (%#018llx)\n",
                             prec->name, ret, prec->val,
                             static_cast<unsigned long long int>(prec->val));
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

// unsigned integer to/from RVAL

template<typename REC>
long
opcua_read_uint32_rval (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pvt->requestOpcuaRead();
        } else {
            ret = pvt->readScalar(&prec->rval, &nextReason);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> RVAL=%u (%#010x)\n",
                             prec->name, ret, prec->rval, prec->rval);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

template<typename REC>
long
opcua_write_uint32_rval (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->tpro > 1)
                errlogPrintf("%s: write <- RVAL=%u (%#010x)\n",
                             prec->name, prec->rval, prec->rval);
            pvt->writeScalar(prec->rval);
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            ret = pvt->readScalar(&prec->rval, &nextReason);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> RVAL=%u (%#010x)\n",
                             prec->name, ret, prec->rval, prec->rval);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

// analog input/output

template<typename REC>
long
opcua_read_analog (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pvt->requestOpcuaRead();
        } else {
            if (prec->linr == menuConvertNO_CONVERSION) {
                double value = 0.;
                ret = pvt->readScalar(&value, &nextReason);
                if (ret == 0) {
                    // Do ASLO/AOFF conversion and smoothing
                    if (prec->aslo != 0.0) value *= prec->aslo;
                    value += prec->aoff;
                    if (prec->smoo == 0.0 || prec->udf || !std::isfinite(prec->val))
                        prec->val = value;
                    else
                        prec->val = prec->val * prec->smoo + value * (1.0 - prec->smoo);
                    prec->udf = 0;
                    ret = 2;     // don't convert
                }
                if (prec->tpro > 1)
                    errlogPrintf("%s: read (status %ld) -> VAL=%g\n",
                                 prec->name, ret, prec->val);
            } else {
                ret = pvt->readScalar(&prec->rval, &nextReason);
                if (prec->tpro > 1)
                    errlogPrintf("%s: read (status %ld) -> RVAL=%d (%#010x)\n",
                                 prec->name, ret, prec->rval,
                                 static_cast<unsigned int>(prec->rval));
            }
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

template<typename REC>
long
opcua_write_analog (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        //TODO: ignore incoming data when output rate limit active
        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->linr == menuConvertNO_CONVERSION) {
                if (prec->tpro > 1) {
                    errlogPrintf("%s: write <- VAL=%g\n",
                                 prec->name, prec->val);
                }
                ret = pvt->writeScalar(prec->val);
            } else {
                if (prec->tpro > 1) {
                    errlogPrintf("%s: write <- RVAL=%d (%#010x)\n",
                                 prec->name, prec->rval,
                                 static_cast<unsigned int>(prec->rval));
                }
                ret = pvt->writeScalar(prec->rval);
            }
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            double value;
            bool useValue = true;
            if (prec->linr == menuConvertNO_CONVERSION) {
                ret = pvt->readScalar(&value, &nextReason);
                if (ret == 0) {
                    if (prec->aslo != 0.0) value *= prec->aslo;
                    value += prec->aoff;
                }
                if (prec->tpro > 1)
                    errlogPrintf("%s: read (status %ld) -> VAL=%g\n",
                                 prec->name, ret, prec->val);
            } else {
                ret = pvt->readScalar(&prec->rval, &nextReason);
                if (ret == 0) {
                    value = static_cast<double>(prec->rval)
                            + static_cast<double>(prec->roff);
                    if (prec->aslo != 0.0) value *= prec->aslo;
                    value += prec->aoff;
                    if (prec->linr == menuConvertLINEAR || prec->linr == menuConvertSLOPE) {
                        value = value * prec->eslo + prec->eoff;
                    } else {
                        if (cvtRawToEngBpt(&value, prec->linr, prec->init,
                                           static_cast<void **>(&prec->pbrk), &prec->lbrk) != 0)
                            useValue = false;
                    }
                }
                if (prec->tpro > 1)
                    errlogPrintf("%s: read (status %ld) -> VAL=%g converted from RVAL=%d (%#010x)\n",
                                 prec->name, ret, value, prec->rval,
                                 static_cast<unsigned int>(prec->rval));
            }
            if (useValue)
                prec->val = value;
            prec->udf = isnan(prec->val);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

// enum output

template<typename REC>
long
opcua_write_enum (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->tpro > 1)
                errlogPrintf("%s: write <- RVAL=%u (%#010x)\n",
                             prec->name, prec->rval, prec->rval);
            pvt->writeScalar(prec->rval);
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            ret = pvt->readScalar(&prec->rval, &nextReason);
            if (ret == 0) {
                epicsUInt32 rval = prec->rval &= prec->mask;
                if (prec->shft > 0)
                    rval >>= prec->shft;
                if (prec->sdef) {
                    epicsUInt32 *pstate_values = &prec->zrvl;
                    epicsUInt16 i;
                    prec->val = 65535;   // unknown state
                    for (i = 0; i < 16; i++) {
                        if (*pstate_values == rval) {
                            prec->val = i;
                            break;
                        }
                        pstate_values++;
                    }
                } else {
                    // no defined states
                    prec->val = static_cast<epicsEnum16>(rval);
                }
            }
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL=%u converted from RVAL=%u (%#010x)\n",
                             prec->name, ret, prec->val, prec->rval, prec->rval);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

// bo output

template<typename REC>
long
opcua_write_bo (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->tpro > 1)
                errlogPrintf("%s: write <- RVAL=%u (%#010x)\n",
                             prec->name, prec->rval, prec->rval);
            pvt->writeScalar(prec->rval);
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            ret = pvt->readScalar(&prec->rval, &nextReason);
            if (ret == 0) {
                if (prec->rval == 0) prec->val = 0;
                else prec->val = 1;
            }
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL=%u (RVAL=%#010x)\n",
                             prec->name, ret, prec->val, prec->rval);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

// mbboDirect output

template<typename REC>
long
opcua_write_mbbod (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->tpro > 1)
                errlogPrintf("%s: write <- RVAL=%u (%#010x)\n",
                             prec->name, prec->rval, prec->rval);
            pvt->writeScalar(prec->rval);
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            ret = pvt->readScalar(&prec->rval, &nextReason);
            if (ret == 0) {
                epicsUInt32 rval = prec->rval;
                if (prec->shft > 0)
                    rval >>= prec->shft;
                prec->val = rval;
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
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL=%u converted from RVAL=%u (%#010x)\n",
                             prec->name, ret, prec->val, prec->rval, prec->rval);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

// string to/from VAL

template<typename REC>
long
opcua_read_string_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pvt->requestOpcuaRead();
        } else {
            ret = pvt->readScalar(prec->val, MAX_STRING_SIZE, &nextReason);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL='%s'\n",
                             prec->name, ret, prec->val);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

template<typename REC>
long
opcua_write_string_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->tpro > 1) {
                errlogPrintf("%s: write <- VAL='%s'\n",
                             prec->name, prec->val);
            }
            ret = pvt->writeScalar(prec->val, MAX_STRING_SIZE);
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            ret = pvt->readScalar(prec->val, MAX_STRING_SIZE, &nextReason);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL='%s'\n",
                             prec->name, ret, prec->val);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

// long string to/from VAL

template<typename REC>
long
opcua_read_lstring_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pvt->requestOpcuaRead();
        } else {
            ret = pvt->readScalar(prec->val, prec->sizv, &nextReason);
            prec->len = static_cast<epicsUInt32>(strlen(prec->val) + 1);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL='%s'\n",
                             prec->name, ret, prec->val);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

template<typename REC>
long
opcua_write_lstring_val (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->tpro > 1) {
                errlogPrintf("%s: write <- VAL='%s'\n",
                             prec->name, prec->val);
            }
            ret = pvt->writeScalar(prec->val, prec->sizv);
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            ret = pvt->readScalar(prec->val, prec->sizv, &nextReason);
            prec->len = static_cast<epicsUInt32>(strlen(prec->val) + 1);
            if (prec->tpro > 1)
                errlogPrintf("%s: read (status %ld) -> VAL='%s'\n",
                             prec->name, ret, prec->val);
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

// waveform to/from VAL

template<typename REC>
long
opcua_read_array (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;
        epicsUInt32 nord = prec->nord;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::readRequest) {
            prec->pact = true;
            pvt->requestOpcuaRead();
        } else {
            switch (prec->ftvl) {
            case menuFtypeSTRING:
                ret = pvt->readArray(static_cast<char *>(prec->bptr), MAX_STRING_SIZE, prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeCHAR:
                ret = pvt->readArray(static_cast<epicsInt8 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUCHAR:
                ret = pvt->readArray(static_cast<epicsUInt8 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeSHORT:
                ret = pvt->readArray(static_cast<epicsInt16 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUSHORT:
                ret = pvt->readArray(static_cast<epicsUInt16 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeLONG:
                ret = pvt->readArray(static_cast<epicsInt32 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeULONG:
                ret = pvt->readArray(static_cast<epicsUInt32 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
#ifdef DBR_INT64
            case menuFtypeINT64:
                ret = pvt->readArray(static_cast<epicsInt64 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUINT64:
                ret = pvt->readArray(static_cast<epicsUInt64 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
#endif
            case menuFtypeFLOAT:
                ret = pvt->readArray(static_cast<epicsFloat32 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeDOUBLE:
                ret = pvt->readArray(static_cast<epicsFloat64 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeENUM:
                ret = pvt->readArray(static_cast<epicsUInt16 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            }
            if (nord != prec->nord)
                db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);

            if (prec->tpro > 1) {
                errlogPrintf("%s: read (status %ld) -> %d array elements read\n",
                             prec->name, ret, prec->nord);
            }
        }
        if (nextReason)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

template<typename REC>
long
opcua_write_array (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);
        ProcessReason nextReason = ProcessReason::none;
        epicsUInt32 nord = prec->nord;

        if (pvt->reason == ProcessReason::none || pvt->reason == ProcessReason::writeRequest) {
            if (prec->tpro > 1) {
                errlogPrintf("%s: write <- %d array elements\n",
                             prec->name, prec->nord);
            }
            switch (prec->ftvl) {
            case menuFtypeSTRING:
                ret = pvt->writeArray(static_cast<char *>(prec->bptr), MAX_STRING_SIZE, prec->nord);
                break;
            case menuFtypeCHAR:
                ret = pvt->writeArray(static_cast<epicsInt8 *>(prec->bptr), prec->nord);
                break;
            case menuFtypeUCHAR:
                ret = pvt->writeArray(static_cast<epicsUInt8 *>(prec->bptr), prec->nord);
                break;
            case menuFtypeSHORT:
                ret = pvt->writeArray(static_cast<epicsInt16 *>(prec->bptr), prec->nord);
                break;
            case menuFtypeUSHORT:
                ret = pvt->writeArray(static_cast<epicsUInt16 *>(prec->bptr), prec->nord);
                break;
            case menuFtypeLONG:
                ret = pvt->writeArray(static_cast<epicsInt32 *>(prec->bptr), prec->nord);
                break;
            case menuFtypeULONG:
                ret = pvt->writeArray(static_cast<epicsUInt32 *>(prec->bptr), prec->nord);
                break;
#ifdef DBR_INT64
            case menuFtypeINT64:
                ret = pvt->writeArray(static_cast<epicsInt64 *>(prec->bptr), prec->nord);
                break;
            case menuFtypeUINT64:
                ret = pvt->writeArray(static_cast<epicsUInt64 *>(prec->bptr), prec->nord);
                break;
#endif
            case menuFtypeFLOAT:
                ret = pvt->writeArray(static_cast<epicsFloat32 *>(prec->bptr), prec->nord);
                break;
            case menuFtypeDOUBLE:
                ret = pvt->writeArray(static_cast<epicsFloat64 *>(prec->bptr), prec->nord);
                break;
            case menuFtypeENUM:
                ret = pvt->writeArray(static_cast<epicsUInt16 *>(prec->bptr), prec->nord);
                break;
            }
            if (pvt->plinkinfo->linkedToItem) {
                prec->pact = true;
                pvt->requestOpcuaWrite();
            }
        } else {
            switch (prec->ftvl) {
            case menuFtypeSTRING:
                ret = pvt->readArray(static_cast<char *>(prec->bptr), MAX_STRING_SIZE, prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeCHAR:
                ret = pvt->readArray(static_cast<epicsInt8 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUCHAR:
                ret = pvt->readArray(static_cast<epicsUInt8 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeSHORT:
                ret = pvt->readArray(static_cast<epicsInt16 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUSHORT:
                ret = pvt->readArray(static_cast<epicsUInt16 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeLONG:
                ret = pvt->readArray(static_cast<epicsInt32 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeULONG:
                ret = pvt->readArray(static_cast<epicsUInt32 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
#ifdef DBR_INT64
            case menuFtypeINT64:
                ret = pvt->readArray(static_cast<epicsInt64 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeUINT64:
                ret = pvt->readArray(static_cast<epicsUInt64 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
#endif
            case menuFtypeFLOAT:
                ret = pvt->readArray(static_cast<epicsFloat32 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeDOUBLE:
                ret = pvt->readArray(static_cast<epicsFloat64 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            case menuFtypeENUM:
                ret = pvt->readArray(static_cast<epicsUInt16 *>(prec->bptr), prec->nelm,
                                     &prec->nord, &nextReason);
                break;
            }
            if (nord != prec->nord)
                db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);

            if (prec->tpro > 1) {
                errlogPrintf("%s: read (status %ld) -> %d array elements\n",
                             prec->name, ret, prec->nord);
            }
        }
        if (nextReason != ProcessReason::none)
            pvt->requestRecordProcessing(nextReason);
    } CATCH();
    return ret;
}

// opcuaItemRecord

template<typename REC>
long
opcua_action_item (REC *prec)
{
    long ret = 0;
    TRY {
        Guard G(pvt->lock);

        switch (pvt->reason) {
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
            pvt->requestOpcuaRead();
            break;
        case ProcessReason::writeRequest:
            prec->pact = true;
            pvt->requestOpcuaWrite();
            break;
        case ProcessReason::none:
            prec->pact = true;
            if (prec->defactn == menuDefActionREAD)
                pvt->requestOpcuaRead();
            else {
                pvt->requestOpcuaWrite();
            }
            break;
        default:
            break;
        }
        pvt->getStatus(&prec->statcode, prec->stattext, MAX_STRING_SIZE+1);
        if (prec->tpro > 1)
            errlogPrintf("%s: processed -> status code %#10x (%s)\n",
                         prec->name, prec->statcode, prec->stattext);
    } CATCH();
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
