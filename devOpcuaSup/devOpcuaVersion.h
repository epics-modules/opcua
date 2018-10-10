/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on code by Michael Davidsaver <mdavidsaver@ospreydcs.com>
 */

#ifndef DEVOPCUA_DEVOPCUAVERSION_H
#define DEVOPCUA_DEVOPCUAVERSION_H

#include <epicsVersion.h>

#ifndef VERSION_INT
#  define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
#endif

/* include generated headers with:
 *   EPICS_OPCUA_MAJOR_VERSION
 *   EPICS_OPCUA_MINOR_VERSION
 *   EPICS_OPCUA_MAINTENANCE_VERSION
 *   EPICS_OPCUA_DEVELOPMENT_FLAG
 */
#include "devOpcuaVersionNum.h"

#define OPCUA_VERSION_INT VERSION_INT(EPICS_OPCUA_MAJOR_VERSION, EPICS_OPCUA_MINOR_VERSION, EPICS_OPCUA_MAINTENANCE_VERSION, 0)

#endif // DEVOPCUA_DEVOPCUAVERSION_H
