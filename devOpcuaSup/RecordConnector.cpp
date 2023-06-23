/*************************************************************************\
* Copyright (c) 2018-2020 ITER Organization.
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

#if defined(_WIN32)
#include <winsock2.h>
#endif

#include <link.h>
#include <shareLib.h>
#include <epicsString.h>
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
        char context[40] = { 0 };
        /* Identify this thread's client from server layer */
        if (dbServerClient(context, sizeof(context))) {
            /* No client, use thread name */
            strncpy(context, epicsThreadGetNameSelf(), sizeof(context));
            context[sizeof(context)-1] = 0;
        }
        printf("%s: Re-process %s\n", context, prec->name);
    }

    status = reinterpret_cast<long (*)(dbCommon *)>(prec->rset->process)(prec);

    if (set_trace)
        *ptrace = 0;

    return status;
}

void processCallback (epicsCallback *pcallback, const ProcessReason reason)
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

void processIncomingDataCallback (epicsCallback *pcallback)
{
    processCallback(pcallback, ProcessReason::incomingData);
}

void processWriteCompleteCallback (epicsCallback *pcallback)
{
    processCallback(pcallback, ProcessReason::writeComplete);
}

void processReadCompleteCallback (epicsCallback *pcallback)
{
    processCallback(pcallback, ProcessReason::readComplete);
}

void processConnectionLossCallback (epicsCallback *pcallback)
{
    processCallback(pcallback, ProcessReason::connectionLoss);
}

void processReadFailureCallback (epicsCallback *pcallback)
{
    processCallback(pcallback, ProcessReason::readFailure);
}

void processWriteFailureCallback (epicsCallback *pcallback)
{
    processCallback(pcallback, ProcessReason::writeFailure);
}

void processReadRequestCallback (epicsCallback *pcallback)
{
    processCallback(pcallback, ProcessReason::readRequest);
}

void processWriteRequestCallback (epicsCallback *pcallback)
{
    processCallback(pcallback, ProcessReason::writeRequest);
}

RecordConnector::RecordConnector (dbCommon *prec)
    : pitem(nullptr)
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
    callbackSetCallback(DevOpcua::processConnectionLossCallback, &connectionLossCallback);
    callbackSetUser(prec, &connectionLossCallback);
    callbackSetCallback(DevOpcua::processReadFailureCallback, &readFailureCallback);
    callbackSetUser(prec, &readFailureCallback);
    callbackSetCallback(DevOpcua::processWriteFailureCallback, &writeFailureCallback);
    callbackSetUser(prec, &writeFailureCallback);
    callbackSetCallback(DevOpcua::processReadRequestCallback, &readRequestCallback);
    callbackSetUser(prec, &readRequestCallback);
    callbackSetCallback(DevOpcua::processWriteRequestCallback, &writeRequestCallback);
    callbackSetUser(prec, &writeRequestCallback);
}

void
RecordConnector::requestRecordProcessing (const ProcessReason reason)
{
    if (debug() > 5)
        std::cout << "Registering record " << getRecordName() << " for processing"
                  << " (" << processReasonString(reason) << ")" << std::endl;
    epicsCallback *callback = nullptr;
    switch (reason) {
    case ProcessReason::none :
    case ProcessReason::incomingData : callback = &incomingDataCallback; break;
    case ProcessReason::writeComplete : callback = &writeCompleteCallback; break;
    case ProcessReason::readComplete : callback = &readCompleteCallback; break;
    case ProcessReason::connectionLoss : callback = &connectionLossCallback; break;
    case ProcessReason::readFailure : callback = &readFailureCallback; break;
    case ProcessReason::writeFailure : callback = &writeFailureCallback; break;
    case ProcessReason::readRequest : callback = &readRequestCallback; break;
    case ProcessReason::writeRequest : callback = &writeRequestCallback; break;
    }
    callbackSetPriority(prec->prio, callback);
    callbackRequest(callback);
}

RecordConnector *
RecordConnector::findRecordConnector (const std::string &name)
{
    RecordConnector *result;
    DBENTRY entry;
    dbInitEntry(pdbbase, &entry);
    if (dbFindRecord(&entry, name.c_str())) {
        dbFinishEntry(&entry);
        return nullptr;
    }
    result = static_cast<RecordConnector *>(static_cast<dbCommon *>(entry.precnode->precord)->dpvt);
    dbFinishEntry(&entry);
    return result;
}

std::set<RecordConnector *>
RecordConnector::glob(const std::string &pattern)
{
    DBENTRY entry;
    long status;
    std::set<RecordConnector *> result;

    dbInitEntry(pdbbase, &entry);
    status = dbFirstRecordType(&entry);
    while (!status) {
        status = dbFirstRecord(&entry);
        while (!status) {
            if (!(dbFindField(&entry, "RTYP") || strcmp(dbGetString(&entry), "opcuaItem"))
                || !(dbFindField(&entry, "DTYP") || dbGetString(&entry) == nullptr
                     || strcmp(dbGetString(&entry), "OPCUA"))) {
                char *pname = dbGetRecordName(&entry);
                RecordConnector *rc = static_cast<RecordConnector *>(
                    static_cast<dbCommon *>(entry.precnode->precord)->dpvt);
                if (rc) {
                    bool match = false;
                    if (epicsStrGlobMatch(pname, pattern.c_str())) {
                        match = true;
                    } else if (rc->plinkinfo) {
                        if (rc->plinkinfo->identifierIsNumeric) {
                            if (epicsStrGlobMatch(std::to_string(rc->plinkinfo->identifierNumber)
                                                      .c_str(),
                                                  pattern.c_str()))
                                match = true;
                        } else {
                            if (epicsStrGlobMatch(rc->plinkinfo->identifierString.c_str(),
                                                  pattern.c_str()))
                                match = true;
                        }
                    }
                    if (match)
                        result.insert(rc);
                }
            }
            status = dbNextRecord(&entry);
        }
        status = dbNextRecordType(&entry);
    }
    dbFinishEntry(&entry);
    return result;
}

} // namespace DevOpcua
