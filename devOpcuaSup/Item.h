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

struct linkInfo;

/**
 * @brief The Item interface for an OPC UA item.
 *
 * The interface provides all item related configuration and functionality.
 */

class Item
{
public:
    virtual ~Item() {}

    virtual void requestRead() = 0;
    virtual void requestWrite() = 0;

    /**
     * @brief Print configuration and status on stdout.
     *
     * The verbosity level controls the amount of information:
     * 0 = one line
     * 1 = item line, then one line per data element
     *
     * @param level  verbosity level
     */
    virtual void show(int level) const = 0;

    virtual bool monitored() const = 0;

    const linkInfo &linkinfo;

protected:
    Item(const linkInfo &info) : linkinfo(info) {}

private:
    Item();
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEM_H
