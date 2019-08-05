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

#include <epicsTypes.h>
#include <epicsTime.h>

struct linkInfo;
class RecordConnector;

/**
 * @brief The Item interface for an OPC UA item.
 *
 * The interface provides all item related configuration and functionality.
 */
class Item
{
public:
    virtual ~Item() {}

    /**
     * @brief Schedule a read request (using beginRead service).
     */
    virtual void requestRead() = 0;

    /**
     * @brief Schedule a write request (using beginWrite service).
     */
    virtual void requestWrite() = 0;

    /**
     * @brief Get the cached status of the last item operation.
     *
     * @param[out] statuscode  OPC UA status code
     * @param[out] statustext  OPC UA status text
     */
    virtual void getStatus(epicsUInt32 *code,
                           epicsOldString *text = nullptr,
                           epicsTimeStamp *ts = nullptr) = 0;

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

    /**
     * @brief Return the monitored status of the item.
     *
     * @return true if item is monitored
     */
    virtual bool isMonitored() const = 0;

    const linkInfo &linkinfo;              /**< configuration of the item as parsed from the EPICS record */
    RecordConnector *itemRecordConnector;  /**< pointer to the itemRecord connector (if linked to one) */

protected:
    /**
     * @brief Constructor for Item, to be used by derived classes.
     *
     * @param info  Item configuration as parsed from EPICS database
     */
    Item(const linkInfo &info)
        : linkinfo(info)
        , itemRecordConnector(nullptr)
    {}
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEM_H
