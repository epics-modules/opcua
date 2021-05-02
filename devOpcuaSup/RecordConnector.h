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

#ifndef RECORDCONNECTOR_H
#define RECORDCONNECTOR_H

#include <memory>
#include <cstddef>
#include <iostream>
#include <set>

#include <epicsMutex.h>
#include <dbCommon.h>
#include <dbScan.h>
#include <recGbl.h>
#include <callback.h>
#include <alarm.h>
#include <menuPriority.h>

#include "devOpcua.h"
#include "DataElement.h"
#include "Item.h"
#include "opcuaItemRecord.h"

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

    long int writeScalar(const char* val, const epicsUInt32 num)
    {
        return pdataelement->writeScalar(val, num, prec);
    }

    template <typename T>
    long int readArray(T *val, const epicsUInt32 num,
                          epicsUInt32 *numRead, ProcessReason *nextReason = nullptr)
    {
        return pdataelement->readArray(val, num, numRead, prec, nextReason);
    }

    long int readArray(char *val, const epicsUInt32 len, const epicsUInt32 num,
                       epicsUInt32 *numRead, ProcessReason *nextReason = nullptr)
    {
        return pdataelement->readArray(val, len, num, numRead, prec, nextReason);
    }

    template <typename T>
    long int writeArray(const T *val, const epicsUInt32 num)
    {
        return pdataelement->writeArray(val, num, prec);
    }

    long int writeArray(const char *val, const epicsUInt32 len, const epicsUInt32 num)
    {
        return pdataelement->writeArray(val, len, num, prec);
    }

    ConnectionStatus state() const { return pitem->state(); }
    void setState(ConnectionStatus state) { pitem->setState(state); }

    void getStatus(epicsUInt32 *code, char *text, const epicsUInt32 len) { pitem->getStatus(code, text, len); }

    void setDataElement(std::shared_ptr<DataElement> data) { pdataelement = data; }
    void clearDataElement() { pdataelement = nullptr; }

    void requestRecordProcessing(const ProcessReason reason);
    void requestOpcuaRead() { pitem->requestRead(); }
    void requestOpcuaWrite() { pitem->requestWrite(); }

    const char *getRecordName() const { return prec->name; }
    const char *getRecordType() const { return prec->rdes->name; }
    menuPriority getRecordPriority() const { return static_cast<menuPriority>(prec->prio); }
    LinkOptionBini bini() const {
        if (plinkinfo->isItemRecord)
            return static_cast<LinkOptionBini>(reinterpret_cast<opcuaItemRecord*>(prec)->bini);
        else
            return plinkinfo->bini;
    }

    int debug() const { return prec->tpro; }

    /**
     * @brief Find record connector by record name.
     *
     * @param name  record name
     * @return pointer to record connector (nullptr if record not found)
     */
    static RecordConnector *findRecordConnector(const std::string &name);

    /**
     * @brief Find record connector with record names matching a glob pattern.
     *
     * @param pattern  record name pattern to match
     * @return set of record connector pointers
     */
    static std::set<RecordConnector *> glob(const std::string &pattern);

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
    CALLBACK readRequestCallback;
    CALLBACK writeRequestCallback;
};

} // namespace DevOpcua

#endif // RECORDCONNECTOR_H
