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

#ifndef RECORDCONNECTOR_H
#define RECORDCONNECTOR_H

#include <epicsMutex.h>
#include <dbCommon.h>

namespace DevOpcua {

class RecordConnector
{
public:
    RecordConnector(dbCommon *prec);

    template<typename VAL> VAL read() const;
    template<typename VAL> void write(VAL val);

    epicsMutex lock;
private:
    dbCommon *prec;
};

} // namespace DevOpcua

#endif // RECORDCONNECTOR_H
