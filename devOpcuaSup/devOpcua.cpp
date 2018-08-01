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
#include <iocsh.h>
#include <errlog.h>
#include <dbScan.h>
#include <drvSup.h>
#include <devSup.h>
#include <recSup.h>
#include <recGbl.h>
#include <cvtTable.h>
#include <menuConvert.h>
#include <alarm.h>

#include <dbCommon.h>
#include <longoutRecord.h>
#include <longinRecord.h>
#include <mbboDirectRecord.h>
#include <mbbiDirectRecord.h>
#include <mbboRecord.h>
#include <mbbiRecord.h>
#include <boRecord.h>
#include <biRecord.h>
#include <aoRecord.h>
#include <aiRecord.h>
#include <waveformRecord.h>
#include <epicsExport.h>  // must be after dbAccess.h (lp:1784616)

#include "devOpcua.h"
#include "RecordConnector.h"
#include "linkParser.h"
#include "ItemUaSdk.h"
#include "DataElementUaSdk.h"

namespace {

using namespace DevOpcua;

#define TRY \
    if (!prec->dpvt) return 0; \
    RecordConnector *pvt = static_cast<RecordConnector*>(prec->dpvt); \
    (void)pvt; \
    try
#define CATCH() catch(std::exception& e) { \
    std::cerr << prec->name << " Error : " << e.what() << std::endl; \
    (void)recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM); \
    return 0; }

// Device Support Extension: link parsing and setup

long
opcua_add_record (dbCommon *prec)
{
    try {
        DBEntry ent(prec);
        std::unique_ptr<RecordConnector> pvt (new RecordConnector(prec));
        pvt->plinkinfo = parseLink(prec, ent);
        //TODO: Switch to implementation selection
        std::unique_ptr<ItemUaSdk> item (new ItemUaSdk(*pvt->plinkinfo));
        item->data().setRecordConnector(pvt.get());
        pvt->pitem = std::move(item);
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
    TRY {
        Guard G(pvt->lock);
        if (pvt->reason == ProcessReason::incomingData) {
            prec->val = pvt->readInt32();
            if (prec->tse == epicsTimeEventDeviceTime)
                prec->time = pvt->readTimeStamp();
            if (prec->tpro > 1) {
                errlogPrintf("%s: read -> VAL=%d (%#010x)\n",
                             prec->name, prec->val,
                             static_cast<unsigned int>(prec->val));
            }
            pvt->clearIncomingData();
        } else if (pvt->reason == ProcessReason::readComplete) {
            pvt->checkReadStatus();
        } else {
            prec->pact = true;
            pvt->requestOpcuaRead();
        }
        return 0;
    } CATCH()
}

template<typename REC>
long
opcua_write_int32_val (REC *prec)
{
    TRY {
        Guard G(pvt->lock);
        if (pvt->reason == ProcessReason::incomingData) {
            prec->val = pvt->readInt32();
            if (prec->tse == epicsTimeEventDeviceTime)
                prec->time = pvt->readTimeStamp();
            if (prec->tpro > 1) {
                errlogPrintf("%s: read -> VAL=%d (%#010x)\n",
                             prec->name, prec->val,
                             static_cast<unsigned int>(prec->val));
            }
            prec->udf = false;
            pvt->clearIncomingData();
        } else if (pvt->reason == ProcessReason::writeComplete) {
            pvt->checkWriteStatus();
        } else {
            if (prec->tpro > 1) {
                errlogPrintf("%s: write <- VAL=%d (%#010x)\n",
                             prec->name, prec->val,
                             static_cast<unsigned int>(prec->val));
            }
            pvt->writeInt32(prec->val);
            prec->pact = true;
            pvt->requestOpcuaWrite();
        }
        return 0;
    } CATCH()
}

// unsigned integer to/from RVAL

template<typename REC>
long
opcua_read_uint32_rval (REC *prec)
{
    TRY {
        Guard G(pvt->lock);
        if (pvt->reason == ProcessReason::incomingData) {
            prec->rval = pvt->readUInt32();
            if (prec->tse == epicsTimeEventDeviceTime)
                prec->time = pvt->readTimeStamp();
            if (prec->tpro > 1) {
                errlogPrintf("%s: read -> RVAL=%u (%#010x)\n",
                             prec->name, prec->rval, prec->rval);
            }
            pvt->clearIncomingData();
        } else if (pvt->reason == ProcessReason::readComplete) {
            pvt->checkReadStatus();
        } else {
            prec->pact = true;
            pvt->requestOpcuaRead();
        }
        return 0;
    } CATCH()
}

template<typename REC>
long
opcua_write_uint32_rval (REC *prec)
{
    TRY {
        Guard G(pvt->lock);
        if (pvt->reason == ProcessReason::incomingData) {
            prec->rval = pvt->readUInt32();
            if (prec->tse == epicsTimeEventDeviceTime)
                prec->time = pvt->readTimeStamp();
            if (prec->tpro > 1) {
                errlogPrintf("%s: read -> RVAL=%u (%#010x)\n",
                             prec->name, prec->rval, prec->rval);
            }
            prec->udf = false;
            pvt->clearIncomingData();
        } else if (pvt->reason == ProcessReason::writeComplete) {
            pvt->checkWriteStatus();
        } else {
            if (prec->tpro > 1) {
                errlogPrintf("%s: write <- RVAL=%d (%#010x)\n",
                             prec->name, prec->rval, prec->rval);
            }
            pvt->writeUInt32(prec->rval);
            prec->pact = true;
            pvt->requestOpcuaWrite();
        }
        return 0;
    } CATCH()
}

// analog input/output

template<typename REC>
long
opcua_read_analog (REC *prec)
{
    TRY {
        long ret = 0;
        Guard G(pvt->lock);
        if (pvt->reason == ProcessReason::incomingData) {
            if (prec->linr == menuConvertNO_CONVERSION) {
                double value = pvt->readFloat64();
                // Do ASLO/AOFF conversion and smoothing
                if (prec->aslo != 0.0) value *= prec->aslo;
                value += prec->aoff;
                if (prec->smoo == 0.0 || prec->udf || !isfinite(prec->val))
                    prec->val = value;
                else
                    prec->val = prec->val * prec->smoo + value * (1.0 - prec->smoo);
                prec->udf = 0;
                ret = 2;     // don't convert
                if (prec->tpro > 1) {
                    errlogPrintf("%s: read -> VAL=%g\n",
                                 prec->name, prec->val);
                }
            } else if (pvt->reason == ProcessReason::readComplete) {
                pvt->checkReadStatus();
            } else {
                prec->rval = pvt->readInt32();
                if (prec->tpro > 1) {
                    errlogPrintf("%s: read -> RVAL=%d (%#010x)\n",
                                 prec->name, prec->rval,
                                 static_cast<unsigned int>(prec->rval));
                }
            }
            if (prec->tse == epicsTimeEventDeviceTime)
                prec->time = pvt->readTimeStamp();
            pvt->clearIncomingData();
        } else {
            prec->pact = true;
            pvt->requestOpcuaRead();
        }
        return ret;
    } CATCH()
}

template<typename REC>
long
opcua_write_analog (REC *prec)
{
    TRY {
        Guard G(pvt->lock);
        //TODO: ignore incoming data when output rate limit active
        if (pvt->reason == ProcessReason::incomingData) {
            double value;
            bool useValue = true;
            if (prec->linr != menuConvertNO_CONVERSION) {
                prec->rval = pvt->readInt32();
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
            } else {
                value = pvt->readFloat64();
                if (prec->aslo != 0.0) value *= prec->aslo;
                value += prec->aoff;
            }
            if (prec->tse == epicsTimeEventDeviceTime)
                prec->time = pvt->readTimeStamp();
            if (useValue) {
                prec->val = value;
                prec->udf = isnan(value);
            }
            if (prec->tpro > 1) {
                errlogPrintf("%s: read -> VAL=%g\n",
                             prec->name, prec->val);
            }
            pvt->clearIncomingData();
        } else if (pvt->reason == ProcessReason::writeComplete) {
            pvt->checkWriteStatus();
        } else {
            if (prec->tpro > 1) {
                errlogPrintf("%s: write <- RVAL=%d (%#010x)\n",
                             prec->name, prec->rval,
                             static_cast<unsigned int>(prec->rval));
            }
            pvt->writeInt32(prec->rval);
            prec->pact = true;
            pvt->requestOpcuaWrite();
        }
        return 0;
    } CATCH()
}

// enum output

template<typename REC>
long
opcua_write_enum (REC *prec)
{
    TRY {
        Guard G(pvt->lock);
        if (pvt->reason == ProcessReason::incomingData) {
            epicsUInt32 rval = pvt->readUInt32() & prec->mask;
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
            prec->udf = FALSE;
            if (prec->tpro > 1) {
                errlogPrintf("%s: read -> VAL=%u (RVAL=%#010x)\n",
                             prec->name, prec->val, prec->rval);
            }
            pvt->clearIncomingData();
        } else if (pvt->reason == ProcessReason::writeComplete) {
            pvt->checkWriteStatus();
        } else {
            if (prec->tpro > 1) {
                errlogPrintf("%s: write <- RVAL=%d (%#010x)\n",
                             prec->name, prec->rval, prec->rval);
            }
            pvt->writeUInt32(prec->rval);
            prec->pact = true;
            pvt->requestOpcuaWrite();
        }
        return 0;
    } CATCH()
}

} // namespace

#define SUPN(NAME, REC, OP, DIR) static dset6<REC##Record> NAME = \
  {6, NULL, opcua_init, NULL, opcua_get_ioint, &opcua_##DIR##_##OP<REC##Record>, NULL}; \
    extern "C" { epicsExportAddress(dset, NAME); }

#define SUPI(NAME, REC, OP, DIR) static dset6<REC##Record> NAME = \
  {6, NULL, opcua_init, opcua_init_mask_##DIR<REC##Record>, opcua_get_ioint, &opcua_##DIR##_##OP<REC##Record>, NULL}; \
    extern "C" { epicsExportAddress(dset, NAME); }

SUPN(devLiOpcua,             longin,   int32_val, read)
SUPN(devLoOpcua,            longout,   int32_val, write)
SUPI(devMbbiOpcua,             mbbi, uint32_rval, read)
SUPI(devMbboOpcua,             mbbo,        enum, write)
SUPI(devMbbiDirectOpcua, mbbiDirect, uint32_rval, read)
SUPI(devMbboDirectOpcua, mbboDirect, uint32_rval, write)
SUPN(devAiOpcua,                 ai,      analog, read)
SUPN(devAoOpcua,                 ao,      analog, write)
