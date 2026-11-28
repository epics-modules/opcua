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

#include <list>

#include <dbCommon.h>

#include "devOpcua.h"
#include "RecordConnector.h"

namespace DevOpcua {

static const char defaultElementDelimiter = '.';

bool getYesNo(const char c);

/**
 * @brief Split configuration string along delimiters into a list<string>.
 *
 * Delimiters at the beginning or end of the string or multiple delimiters in a row
 * generate empty list elements.
 *
 * @param str  string to split
 * @param delim  token delimiter
 *
 * @return  tokens in order of appearance as list<string>
 */
std::list<std::string> splitString(const std::string &str,
                                   const char delim = defaultElementDelimiter);

/**
 * @brief Parse a browse path string into a list of BrowsePathElements.
 *
 * Syntax (subset of the OPC UA relative path text format):
 * elements are separated by '/' (a leading '/' is optional),
 * each element is a BrowseName with an optional "<nsIndex>:" prefix,
 * '&' escapes the following character (e.g. "&/", "&:", "&&").
 *
 * @param str  browse path string
 *
 * @return  list of path elements
 * @throws std::runtime_error  on syntax errors
 */
std::list<BrowsePathElement> parseBrowsePath(const std::string &str);

/**
 * @brief Return a printable description of the node configuration of a link.
 */
std::string nodeSpecString(const linkInfo &info);

std::unique_ptr<linkInfo> parseLink(dbCommon *prec, const DBEntry &ent);

} // namespace DevOpcua

#endif // DEVOPCUA_LINKPARSER_H
