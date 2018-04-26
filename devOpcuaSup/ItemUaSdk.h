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

#ifndef DEVOPCUA_ITEMUASDK_H
#define DEVOPCUA_ITEMUASDK_H

#include "Item.h"

namespace DevOpcua {

using namespace UaClientSdk;

class ItemUaSdk : public Item
{
public:
    ItemUaSdk();
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEMUASDK_H
