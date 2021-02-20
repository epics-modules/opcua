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

static const char defaultElementDelimiter = '.';

bool getYesNo(const char c);

/**
 * @brief Split configuration string along delimiters into a vector<string>.
 *
 * Delimiters at the beginning or end of the string or multiple delimiters in a row
 * generate empty vector elements.
 *
 * @param str  string to split
 * @param delim  token delimiter
 *
 * @return  tokens in order of appearance as vector<string>
 */
std::vector<std::string> splitString(const std::string &str,
                                     const char delim = defaultElementDelimiter);

std::unique_ptr<linkInfo> parseLink(dbCommon *prec, const DBEntry &ent);

} // namespace DevOpcua

#endif // DEVOPCUA_LINKPARSER_H
