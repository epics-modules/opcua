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

#include <epicsVersion.h>
#include <epicsStdlib.h>
#include <epicsTypes.h>
#include <devSup.h>
#include <drvSup.h>
#include <recGbl.h>
#include <alarm.h>
#include <dbAccess.h>
#include <epicsExit.h>
#include <iocsh.h>
#include <errlog.h>

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
#include <devSup.h>
#include <epicsExport.h>

#include "devOpcua.h"
#include "RecordConnector.h"
#include "linkParser.h"
#include "ItemUaSdk.h"

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

long opcua_add_record (dbCommon *prec)
{
    try {
        DBEntry ent(prec);
        std::unique_ptr<RecordConnector> pvt (new RecordConnector(prec));
        pvt->plinkinfo = parseLink(prec, ent);
        //TODO: Switch to implementation selection
        std::unique_ptr<ItemUaSdk> item (new ItemUaSdk(*pvt->plinkinfo));
        pvt->pitem = std::move(item);
        prec->dpvt = pvt.release();
        return 0;
    } catch(std::exception& e) {
        std::cerr << prec->name << " Error in add_record : " << e.what() << std::endl;
        return S_dbLib_badLink;
    }
}

long opcua_del_record (dbCommon *prec)
{
    //TODO: support changing OPCUA links
    return -1;
}

dsxt opcua_dsxt = { opcua_add_record, opcua_del_record };

// initialization

long opcua_init (int pass)
{
    if (pass == 0) devExtend(&opcua_dsxt);
    return 0;
}

// integer to/from VAL

template<typename REC>
long opcua_read_int_val (REC *prec)
{
    TRY {
        Guard G(pvt->lock);
        prec->val = pvt->read<epicsUInt32>();
        if (prec->tpro > 1) {
            errlogPrintf("%s: read -> VAL=%08x\n", prec->name, (unsigned)prec->val);
        }
        return 0;
    } CATCH()
}

template<typename REC>
long opcua_write_int_val (REC *prec)
{
    TRY {
        Guard G(pvt->lock);
        if (prec->tpro > 1) {
            errlogPrintf("%s: write <- VAL=%08x\n", prec->name, (unsigned)prec->val);
        }
        pvt->write(prec->val);
        return 0;
    } CATCH()
}

} // namespace

#define SUP(NAME, REC, OP, DIR) static dset6<REC##Record> NAME = \
  {6, NULL, opcua_init, NULL, NULL, &opcua_##DIR##_##OP<REC##Record>, NULL}; \
    extern "C" { epicsExportAddress(dset, NAME); }

SUP(devLiOpcua, longin,  int_val, read)
SUP(devLoOpcua, longout, int_val, write)
