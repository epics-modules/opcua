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

#ifndef RECORDCONNECTOR_H
#define RECORDCONNECTOR_H

#include <memory>

#include <epicsMutex.h>
#include <dbCommon.h>

#include "devOpcua.h"
#include "DataElement.h"

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

    template<typename VAL> VAL read() const;
    template<typename VAL> void write(VAL val);

    epicsMutex lock;
    std::unique_ptr<linkInfo> plinkinfo;
private:
    DataElement *pdataelement;
    dbCommon *prec;
};

template<typename VAL>
VAL RecordConnector::read() const
{
    epicsUInt32 OV(0);
    return OV;
}

template<typename VAL>
void RecordConnector::write(VAL val)
{
    (void)val;
}

} // namespace DevOpcua

#endif // RECORDCONNECTOR_H
