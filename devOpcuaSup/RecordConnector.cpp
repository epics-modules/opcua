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

#include "RecordConnector.h"

namespace DevOpcua {

RecordConnector::RecordConnector (dbCommon *prec)
    : prec(prec)
{}

} // namespace DevOpcua
