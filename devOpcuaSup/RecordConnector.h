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

#ifndef RECORDCONNECTOR_H
#define RECORDCONNECTOR_H

#include <memory>

#include <epicsMutex.h>
#include <dbCommon.h>
#include <dbScan.h>
#include <callback.h>

#include "devOpcua.h"
#include "DataElement.h"
#include "Item.h"

namespace DevOpcua {

class RecordConnector
{
public:
    RecordConnector(dbCommon *prec);

    epicsTimeStamp readTimeStamp() const { return pdataelement->readTimeStamp(plinkinfo->useServerTimestamp); }

    epicsInt32 readInt32() const { return pdataelement->readInt32(); }
    void writeInt32(const epicsInt32 val) const { pdataelement->writeInt32(val); }
    epicsUInt32 readUInt32() const { return pdataelement->readUInt32(); }
    void writeUInt32(const epicsUInt32 val) const { pdataelement->writeUInt32(val); }
    epicsFloat64 readFloat64() const { return pdataelement->readFloat64(); }
    void writeFloat64(const epicsFloat64 val) const { pdataelement->writeFloat64(val); }

    void clearIncomingData() { pdataelement->clearIncomingData(); }
    void checkWriteStatus() const;

    void setDataElement(DataElement *data) { pdataelement = data; }
    void clearDataElement() { pdataelement = NULL; }

    void requestRecordProcessing(const ProcessReason reason);
    void requestOpcuaRead() { pitem->requestRead(); }
    void requestOpcuaWrite() { pitem->requestWrite(); }

    const char * getRecordName() { return prec->name; }
    const int debug() const { return ((prec->tpro > 0) ? prec->tpro - 1 : 0); }

    epicsMutex lock;
    std::unique_ptr<linkInfo> plinkinfo;
    std::unique_ptr<Item> pitem;
    DataElement *pdataelement;
    bool isIoIntrScanned;
    IOSCANPVT ioscanpvt;
    ProcessReason reason;
private:
    dbCommon *prec;
    CALLBACK incomingDataCallback;
    CALLBACK readCompleteCallback;
    CALLBACK writeCompleteCallback;
};

} // namespace DevOpcua

#endif // RECORDCONNECTOR_H
