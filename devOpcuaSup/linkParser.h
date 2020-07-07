/*************************************************************************\
* Copyright (c) 2018-2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on code by Michael Davidsaver <mdavidsaver@ospreydcs.com>
 */

#ifndef DEVOPCUA_LINKPARSER_H
#define DEVOPCUA_LINKPARSER_H

#include <dbCommon.h>

#include "devOpcua.h"
#include "RecordConnector.h"

namespace DevOpcua {

bool getYesNo(const char c);

std::unique_ptr<linkInfo> parseLink(dbCommon* prec, const DBEntry &ent);

} // namespace DevOpcua

#endif // DEVOPCUA_LINKPARSER_H
