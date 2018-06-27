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

#ifndef DEVOPCUA_ITEM_H
#define DEVOPCUA_ITEM_H

namespace DevOpcua {

/**
 * @brief The Item interface for an OPC UA item.
 *
 * The interface provides all item related configuration and functionality.
 */

class Item
{
public:
    Item();
    virtual ~Item() {}

    virtual bool monitored() const = 0;
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEM_H
