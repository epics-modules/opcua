/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 */

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string.h>

#include <link.h>
#include <shareLib.h>
#include <epicsThread.h>
#include <callback.h>
#include <recSup.h>
#include <recGbl.h>
#include <dbLock.h>
#include <dbScan.h>
#include <dbServer.h>
#include <dbAccessDefs.h>
#include <dbStaticLib.h>
#include <errlog.h>
#include <alarm.h>

#define epicsExportSharedSymbols
#include "RecordConnector.h"
#include "Session.h"

namespace DevOpcua {

// See dbProcess() in dbAccess.c
long reProcess (dbCommon *prec)
{
    char context[40] = "";
    long status = 0;
    int *ptrace;
    bool set_trace = false;

    ptrace = dbLockSetAddrTrace(prec);

    /* check for trace processing*/
    if (prec->tpro) {
        if (!*ptrace) {
            *ptrace = 1;
            set_trace = true;
        }
    }

    if (*ptrace) {
        /* Identify this thread's client from server layer */
        if (dbServerClient(context, sizeof(context))) {
            /* No client, use thread name */
            strncpy(context, epicsThreadGetNameSelf(), sizeof(context));
            context[sizeof(context) - 1] = 0;
        }
        printf("%s: Re-process %s\n", context, prec->name);
    }

    status = reinterpret_cast<long (*)(dbCommon *)>(prec->rset->process)(prec);

    if (set_trace)
        *ptrace = 0;

    return status;
}

void processCallback (CALLBACK *pcallback, const ProcessReason reason)
{
    void *pUsr;
    dbCommon *prec;

    callbackGetUser(pUsr, pcallback);
    prec = static_cast<dbCommon *>(pUsr);
    if (!prec || !prec->dpvt) return;

    RecordConnector *pvt = static_cast<RecordConnector*>(prec->dpvt);
    dbScanLock(prec);
    ProcessReason oldreason = pvt->reason;
    pvt->reason = reason;
    if (prec->pact)
        reProcess(prec);
    else
        dbProcess(prec);
    pvt->reason = oldreason;
    dbScanUnlock(prec);
}

void processIncomingDataCallback (CALLBACK *pcallback)
{
    processCallback(pcallback, ProcessReason::incomingData);
}

void processWriteCompleteCallback (CALLBACK *pcallback)
{
    processCallback(pcallback, ProcessReason::writeComplete);
}

void processReadCompleteCallback (CALLBACK *pcallback)
{
    processCallback(pcallback, ProcessReason::readComplete);
}

RecordConnector::RecordConnector (dbCommon *prec)
    : isIoIntrScanned(false)
    , reason(ProcessReason::none)
    , prec(prec)
{
    scanIoInit(&ioscanpvt);
    callbackSetCallback(DevOpcua::processIncomingDataCallback, &incomingDataCallback);
    callbackSetUser(prec, &incomingDataCallback);
    callbackSetCallback(DevOpcua::processReadCompleteCallback, &readCompleteCallback);
    callbackSetUser(prec, &readCompleteCallback);
    callbackSetCallback(DevOpcua::processWriteCompleteCallback, &writeCompleteCallback);
    callbackSetUser(prec, &writeCompleteCallback);
}

void
RecordConnector::requestRecordProcessing (const ProcessReason reason)
{
    CALLBACK *callback;

    if (isIoIntrScanned && reason == ProcessReason::incomingData) {
        this->reason = reason;
        scanIoRequest(ioscanpvt);
    } else {
        if (reason == ProcessReason::writeComplete)
            callback = &writeCompleteCallback;
        else if (reason == ProcessReason::readComplete)
            callback = &readCompleteCallback;
        else
            callback = &incomingDataCallback;
        callbackSetPriority(prec->prio, callback);
        callbackRequest(callback);
    }
}

void
RecordConnector::checkWriteStatus() const
{
    if (!pdataelement->writeWasOk()) {
        if (debug() >= 2)
            errlogPrintf("%s: write returned an error -> setting to WRITE/INVALID\n",
                         prec->name);
        (void)recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
    }
}

void
RecordConnector::checkReadStatus() const
{
    if (!pdataelement->readWasOk()) {
        if (debug() >= 2)
            errlogPrintf("%s: read returned an error -> setting to READ/INVALID\n",
                         prec->name);
        (void)recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
    }
}

} // namespace DevOpcua
