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

#include <epicsVersion.h>
#include <epicsStdlib.h>
#include <epicsTypes.h>
#include <devSup.h>
#include <drvSup.h>
#include <recGbl.h>
#include <alarm.h>
#include <dbAccess.h>
#include <epicsExit.h>
#include <epicsTime.h>
#include <iocsh.h>
#include <errlog.h>

#include <dbCommon.h>
#include <dbScan.h>
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
#include <devSup.h>
#include <epicsExport.h>

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
        if (pvt->reason == ProcessReason::incomingData
                || pvt->reason == ProcessReason::readComplete) {
            prec->val = pvt->readInt32();
            if (prec->tse == epicsTimeEventDeviceTime)
                prec->time = pvt->readTimeStamp();
            if (prec->tpro > 1) {
                errlogPrintf("%s: read -> VAL=%d (%08x)\n",
                             prec->name, prec->val,
                             static_cast<unsigned int>(prec->val));
            }
            pvt->clearIncomingData();
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
                errlogPrintf("%s: read -> VAL=%d (%08x)\n",
                             prec->name, prec->val,
                             static_cast<unsigned int>(prec->val));
            }
            prec->udf = false;
            pvt->clearIncomingData();
        } else if (pvt->reason == ProcessReason::writeComplete) {
            pvt->checkWriteStatus();
        } else {
            if (prec->tpro > 1) {
                errlogPrintf("%s: write <- VAL=%d (%08x)\n",
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

} // namespace

#define SUP(NAME, REC, OP, DIR) static dset6<REC##Record> NAME = \
  {6, NULL, opcua_init, NULL, opcua_get_ioint, &opcua_##DIR##_##OP<REC##Record>, NULL}; \
    extern "C" { epicsExportAddress(dset, NAME); }

SUP(devLiOpcua, longin,  int32_val, read)
SUP(devLoOpcua, longout, int32_val, write)
