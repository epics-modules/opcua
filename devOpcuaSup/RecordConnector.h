/*************************************************************************\
* Copyright (c) 2018-2019 ITER Organization.
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
#include <cstddef>
#include <iostream>

#include <epicsMutex.h>
#include <dbCommon.h>
#include <dbScan.h>
#include <recGbl.h>
#include <callback.h>
#include <alarm.h>

#include "devOpcua.h"
#include "DataElement.h"
#include "Item.h"

namespace DevOpcua {

class RecordConnector
{
public:
    RecordConnector(dbCommon *prec);

    template <typename T>
    long int readScalar(T *val, ProcessReason *nextReason = nullptr)
    {
        return pdataelement->readScalar(val, prec, nextReason);
    }

    long int readScalar(char *val, const epicsUInt32 num, ProcessReason *nextReason = nullptr)
    {
        return pdataelement->readScalar(val, num, prec, nextReason);
    }

    template <typename T>
    long int writeScalar(const T val) const
    {
        return pdataelement->writeScalar(val, prec);
    }

    template <typename T>
    long int readArray(T *val, const epicsUInt32 num,
                          epicsUInt32 *numRead, ProcessReason *nextReason = nullptr)
    {
        return pdataelement->readArray(val, num, numRead, prec, nextReason);
    }

    template <typename T>
    long int writeArray(const T *val, const epicsUInt32 num)
    {
        return pdataelement->writeArray(val, num, prec);
    }

    void getStatus(epicsUInt32 *code, epicsOldString *text) { pitem->getStatus(code, text, &prec->time); }

    void setDataElement(std::shared_ptr<DataElement> data) { pdataelement = data; }
    void clearDataElement() { pdataelement = nullptr; }

    void requestRecordProcessing(const ProcessReason reason);
    void requestOpcuaRead() { pitem->requestRead(); }
    void requestOpcuaWrite() { pitem->requestWrite(); }

    const char *getRecordName() const { return prec->name; }
    const char *getRecordType() const { return prec->rdes->name; }
    int debug() const { return prec->tpro; }

    /**
     * @brief Find record connector by record name.
     *
     * @param name  record name
     * @return record connector (nullptr if record not found)
     */
    static RecordConnector *findRecordConnector(const std::string &name);

    epicsMutex lock;
    std::unique_ptr<linkInfo> plinkinfo;
    Item *pitem;
    std::shared_ptr<DataElement> pdataelement;
    bool isIoIntrScanned;
    IOSCANPVT ioscanpvt;
    ProcessReason reason;
private:
    dbCommon *prec;
    CALLBACK incomingDataCallback;
    CALLBACK readCompleteCallback;
    CALLBACK writeCompleteCallback;
    CALLBACK connectionLossCallback;
    CALLBACK readFailureCallback;
    CALLBACK writeFailureCallback;
};

} // namespace DevOpcua

#endif // RECORDCONNECTOR_H
