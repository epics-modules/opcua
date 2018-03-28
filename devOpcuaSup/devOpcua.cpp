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

namespace {

using namespace DevOpcua;

#define TRY if (!prec->dpvt) return 0; RecordConnector *pvt = static_cast<RecordConnector*>(prec->dpvt); (void)pvt; try
#define CATCH() catch(std::exception& e) { std::cerr<<prec->name << " Error : " << e.what() << "\n"; (void)recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM); return 0; }

// initialization

long opcua_init_record(dbCommon *prec)
{
    try {
        DBEntry ent(prec);
        std::unique_ptr<RecordConnector> pvt (new RecordConnector(prec));

        prec->dpvt = pvt.release();
        return 0;
    } catch(std::exception& e) {
        std::cerr << prec->name << " Error in init_record " << e.what() << "\n";
        return S_db_errArg;
    }
}

// integer to/from VAL

template<typename REC>
long opcua_read_int_val(REC *prec)
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
long opcua_write_int_val(REC *prec)
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
  {6, NULL, NULL, opcua_init_record, NULL, &opcua_##DIR##_##OP<REC##Record>, NULL}; \
    epicsExportAddress(dset, NAME)

SUP(devOpcuaLi, longin,  int_val, read);
SUP(devOpcuaLo, longout, int_val, write);
