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

/**
 * @brief Enum for the EPICS related state of an OPC UA item
 */
enum ConnectionStatus { down, initialRead, initialWrite, up };

inline const char *
connectionStatusString (const ConnectionStatus status) {
    switch(status) {
    case down:         return "down";
    case initialRead:  return "initialRead";
    case initialWrite: return "initialWrite";
    case up:           return "up";
    }
    return "Illegal Value";
}

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
     * @brief Factory method to dynamically create an Item of the specific implementation.
     *
     * @param info  linkinfo data of the parsed record link
     * @return  pointer to the Item base class of the created object
     */
    static Item * newItem(const linkInfo &info);

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
     * @param[out] code  OPC UA status code
     * @param[out] text  OPC UA status text (will be null terminated)
     * @param[in]  len  Length of text buffer
     */
    virtual void getStatus(epicsUInt32 *code,
                           char *text = nullptr,
                           const epicsUInt32 len = 0,
                           epicsTimeStamp *ts = nullptr) = 0;

    /**
     * @brief Get the EPICS-related state of the item.
     *
     * @return connection state (EPICS-related)
     */
    virtual ConnectionStatus state() const = 0;

    /**
     * @brief Set the EPICS-related state of the item.
     *
     * @param newState  state to be set
     */
    virtual void setState(const ConnectionStatus state) = 0;

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

    const linkInfo &linkinfo;           /**< configuration of the item as parsed from the EPICS record */
    RecordConnector *recConnector;      /**< pointer to the relevant recordConnector */

protected:
    /**
     * @brief Constructor for Item, to be used by derived classes.
     *
     * @param info  Item configuration as parsed from EPICS database
     */
    Item(const linkInfo &info)
        : linkinfo(info)
        , recConnector(nullptr)
    {}
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEM_H
