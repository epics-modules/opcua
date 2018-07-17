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

#include <link.h>
#include <epicsExport.h>
#include <callback.h>
#include <recSup.h>
#include <dbLock.h>
#include <dbScan.h>

#include "RecordConnector.h"
#include "Session.h"

namespace DevOpcua {

static void processIncomingDataCallback (CALLBACK *pcallback)
{
    void *pUsr;
    dbCommon *prec;

    callbackGetUser(pUsr, pcallback);
    prec = static_cast<dbCommon *>(pUsr);
    if (!prec || !prec->dpvt) return;

    RecordConnector *pvt = static_cast<RecordConnector*>(prec->dpvt);
    dbScanLock(prec);
    pvt->incomingData = true;
    if (prec->pact)
        reinterpret_cast<long (*)(dbCommon *)>(prec->rset->process)(prec);
    else
        dbProcess(prec);
    pvt->incomingData = false;
    dbScanUnlock(prec);
}

RecordConnector::RecordConnector (dbCommon *prec)
    : isIoIntrScanned(false)
    , incomingData(false)
    , prec(prec)
{
    scanIoInit(&ioscanpvt);
    callbackSetCallback(DevOpcua::processIncomingDataCallback, &callback);
    callbackSetPriority(prec->prio, &callback);
    callbackSetUser(prec, &callback);
}

void
RecordConnector::requestRecordProcessing ()
{
    if (isIoIntrScanned)
        scanIoRequest(ioscanpvt);
    else
        callbackRequest(&callback);
}

} // namespace DevOpcua
