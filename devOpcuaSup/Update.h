/*************************************************************************\
* Copyright (c) 2019 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#ifndef DEVOPCUA_UPDATE_H
#define DEVOPCUA_UPDATE_H

#include <memory>
#include <utility>
#include <queue>

#include <epicsTime.h>

#include "devOpcua.h"

namespace DevOpcua {

/**
 * @brief An update for an OPC UA Data Element.
 *
 * An update is created for every Data Element after results of an
 * OPC UA service call have been received, or as a result of a
 * special situation (e.g. connection loss).
 *
 * It consists of the mandatory parts
 * - (EPICS) time stamp
 * - update type (ProcessReason for the update)
 *
 * and the optional (implementation dependent type) parts
 * - data object (using a std::unique_ptr)
 * - status code
 *
 * Update uses C++11 std::unique_ptr to manage the data object, i.e.
 * the data is owned by the Update until it goes out of scope.
 * The status code is assumed to be small, i.e. the minimal raw type
 * that holds an OPC UA status.
 */

template<typename T, typename S>
class Update
{
public:
    /**
     * @brief Constructor with const reference for data.
     *
     * This constructor creates a new unique_ptr managed data object,
     * creating a copy of the data.
     *
     * @param time  EPICS time stamp of this update
     * @param type  type of the update (process reason)
     * @param data  const reference to the data to put in the update
     * @param status  status code related to the update
     */
    Update(const epicsTime &time, ProcessReason reason,
           const T &newdata, S status)
        : overrides(0)
        , ts(time)
        , type(reason)
        , data(std::unique_ptr<T>(new T(newdata)))
        , status(status)
    {}

    /**
     * @brief Constructor with unique_ptr for data.
     *
     * This constructor moves a unique_ptr managed data rvalue
     * into the Update.
     *
     * @param ts  EPICS time stamp of this update
     * @param type  type of the update (process reason)
     * @param newdata  data to put in the update
     * @param status  status code related to the update
     */
    Update(const epicsTime &time, const ProcessReason reason,
           std::unique_ptr<T> newdata, S status)
        : overrides(0)
        , ts(time)
        , type(reason)
        , data(std::move(newdata))
        , status(status)
    {}

    /**
     * @brief Constructor with no data, for service
     * results without data.
     *
     * @param ts  EPICS time stamp of this update
     * @param type  type of the update (process reason)
     * @param status  status code related to the update
     */
    Update(const epicsTime &time, const ProcessReason reason)
        : overrides(0)
        , ts(time)
        , type(reason)
        , status()
    {}

    /**
     * @brief Override an update with data.
     *
     * Overrides the update object, increasing the overrides counter,
     * replacing its data and time stamp with a copy of the specified
     * other update and moving the other update's data into this
     *
     * This is used to drop this update, replacing it with the
     * (newer) update behind it.
     *
     * @param other  update whose data is used to replace this
     */
    void override(Update<T, S> &other)
    {
        ts = other.getTimeStamp();
        type = other.getType();
        overrides += other.getOverrides() + 1;
        data = other.releaseData();
        status = other.getStatus();
    }

    /**
     * @brief Override an update without data.
     *
     * Adds the argument to its overrides counter.
     *
     * This is used to carry over the overrides counter if the
     * (older) update in front of this one was dropped.
     *
     * @param count  number to step up the overrides counter
     */
    void override(unsigned long count)
    {
        overrides += count + 1;
    }

    /**
     * @brief Getter for the update's EPICS time stamp.
     *
     * @return  EPICS time stamp of the update
     */
    epicsTime getTimeStamp() const { return ts; }

    /**
     * @brief Getter for the update's type.
     *
     * @return  type of the update
     */
    ProcessReason getType() const { return type; }

    /**
     * @brief Mover for the update's data.
     *
     * Ownership is moved from update to the caller.
     *
     * @return  unique_ptr to the update data
     */
    std::unique_ptr<T> releaseData() { return std::move(data); }

    /**
     * @brief Getter for the update's data.
     *
     * Ownership is retained by the update.
     * Undefined if the Update has no data.
     *
     * @return  reference to the update's data
     */
    T& getData() const { return *data; }

    /**
     * @brief Getter for the update's status.
     *
     * Undefined if the Update has no data.
     *
     * @return  update's status
     */
    S getStatus() const { return status; }

    /**
     * @brief Getter for the update's overrides counter.
     *
     * @return  overrides counter
     */
    unsigned long getOverrides() const { return overrides; }

    /**
     * @brief Checks if the update contains data.
     *
     * Checks if this->data stores a pointer, i.e. whether getData() is
     * defined.
     */
    explicit operator bool() const noexcept { return bool(data); }

private:
    unsigned long overrides;
    epicsTime ts;
    ProcessReason type;
    std::unique_ptr<T> data;
    S status;
};

} // namespace DevOpcua

#endif // DEVOPCUA_UPDATE_H
