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

/** @brief Configuration data for a single record instance.
 *
 * This structure holds all configuration data for a single instance of
 * a supported standard record type, i.e. the result of INP/OUT link parsing.
 *
 * It is kept around, as in the case of a server disconnect/reconnect, the
 * complete sequence of creating the lower level interface will have to be
 * repeated.
 */
typedef struct linkInfo {
    bool useSimpleSetup = true;
    std::string session;
    std::string subscription;

    epicsUInt16 namespaceIndex = 0;
    bool identifierIsNumeric = false;
    epicsUInt32 identifierNumber;
    std::string identifierString;

    double samplingInterval;
    epicsUInt32 queueSize;
    bool discardOldest = true;

    std::string element;
    bool useServerTimestamp = true;

    bool isOutput;
    bool doOutputReadback = true;
} linkInfo;

typedef std::unique_ptr<linkInfo> (*linkParserFunc)(dbCommon*, DBEntry&);

class RecordConnector
{
public:
    RecordConnector(dbCommon *prec);

    epicsTimeStamp readTimeStamp() const { return pdataelement->readTimeStamp(plinkinfo->useServerTimestamp); }
    epicsInt32 readInt32() const { return pdataelement->readInt32(); }
    void writeInt32(const epicsInt32 val) const {}
    void clearIncomingData() { pdataelement->clearIncomingData(); }

    void setDataElement(DataElement *data) { pdataelement = data; }
    void clearDataElement() { pdataelement = NULL; }

    void requestRecordProcessing();
    void requestOpcuaRead() { pitem->requestRead(); }
    void requestOpcuaWrite() { pitem->requestWrite(); }

    const char * getRecordName() { return prec->name; }
    const int debug() { return ((prec->tpro > 0) ? prec->tpro - 1 : 0); }

    epicsMutex lock;
    std::unique_ptr<linkInfo> plinkinfo;
    std::unique_ptr<Item> pitem;
    DataElement *pdataelement;
    bool isIoIntrScanned;
    IOSCANPVT ioscanpvt;
    bool incomingData;
private:
    dbCommon *prec;
    CALLBACK callback;
};

} // namespace DevOpcua

#endif // RECORDCONNECTOR_H
