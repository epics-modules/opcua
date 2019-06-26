/*************************************************************************\
* Copyright (c) 2019 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#ifndef DEVOPCUA_UPDATEQUEUE_H
#define DEVOPCUA_UPDATEQUEUE_H

#include <memory>
#include <utility>
#include <queue>

#include <epicsTime.h>

#include "devOpcua.h"

namespace DevOpcua {

/**
 * @brief An update (incoming data, connection loss, etc).
 *
 * Consists of a mandatory (EPICS) time stamp,
 * a mandatory type (reason for this update),
 * and a data object (of implementation dependent type)
 *
 * Uses C++11 unique_ptr. The data is owned
 * by the Update object while being on the queue.
 */
template<class T>
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
     */
    Update(const epicsTime &time, ProcessReason reason, const T &newdata)
        : overrides(0)
        , ts(time)
        , type(reason)
        , data(std::unique_ptr<T>(new T(newdata)))
    {}
    /**
     * @brief Constructor with unique_ptr for data.
     *
     * This constructor takes a unique_ptr managed data rvalue.
     *
     * @param ts  EPICS time stamp of this update
     * @param type  type of the update (process reason)
     * @param newdata  data to put in the update
     */
    Update(const epicsTime &time, const ProcessReason reason, std::unique_ptr<T> newdata)
        : overrides(0)
        , ts(time)
        , type(reason)
        , data(std::move(newdata))
    {}
    /**
     * @brief Constructor with no data, for connection and status events.
     *
     * @param ts  EPICS time stamp of this update
     * @param type  type of the update (process reason)
     */
    Update(const epicsTime &time, const ProcessReason reason)
        : overrides(0)
        , ts(time)
        , type(reason)
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
    void override(Update<T> &other)
    {
        ts = other.getTimeStamp();
        type = other.getType();
        overrides += other.getOverrides() + 1;
        data = other.releaseData();
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
};

/**
 * @brief A fixed size queue for handling incoming updates (data and events).
 *
 * When updates are pushed to a full queue, either the front or the back update
 * on the queue (depending on the queue's discard policy) are dropped
 * and the overrides counter of the following update is stepped up.
 *
 * C++11 shared_ptr is used for managing the updates on the queue.
 * This allows to e.g. always cache a pointer to the latest update
 * while all updates go through the queue and are consumed at
 * the other end.

 */
template<class T>
class UpdateQueue
{
public:
    UpdateQueue(const size_t size, const bool discardOldest = true)
        : maxElements(size)
        , discardOldest(discardOldest)
    {}

    /**
     * @brief Inserts an update at the end.
     *
     * Pushes the given update to the end of the queue.
     *
     * @param update  the update to push
     * @param[out] wasFirst  `true` if pushed element was the first one, `false` otherwise
     */
    void pushUpdate(std::shared_ptr<Update<T>> update, bool *wasFirst = nullptr)
    {
        Guard G(lock);
        if (wasFirst) *wasFirst = false;
        if (updq.size() < maxElements) {
            if (wasFirst && updq.empty()) *wasFirst = true;
            updq.push(update);
        } else {
            if (discardOldest) {
                std::shared_ptr<Update<T>> drop = updq.front();
                updq.pop();
                updq.front()->override(drop->getOverrides());
                updq.push(update);
            } else {
                updq.back()->override(*update);
            }
        }
    }

    /**
     * @brief Removes an update from the front.
     *
     * Removes an update from the front of the underlying
     * queue and returns it.
     *
     * Calling popUpdate on an empty queue is undefined.
     *
     * @param[out] nextReason  ProcessReason of the next element, `none` if last element
     *
     * @return  reference to the removed update
     */
    std::shared_ptr<Update<T>> popUpdate(ProcessReason *nextReason = nullptr)
    {
        Guard G(lock);
        std::shared_ptr<Update<T>> drop = updq.front();
        updq.pop();
        if (nextReason) {
            if (updq.empty()) *nextReason = ProcessReason::none;
            else *nextReason = updq.front()->getType();
        }
        return drop;
    }

    /**
     * @brief Checks whether the queue is empty.
     *
     * Checks if the underlying queue has no elements,
     * i.e. whether `updq.empty()`.
     *
     * @return  `true` if the queue is empty, `false` otherwise
     */
    bool empty() const { return updq.empty(); }

    /**
     * @brief Returns the number of elements.
     *
     * Returns the number of elements in the underlying queue,
     * i.e., `updq.size()`.
     *
     * @return  number of elements in the queue
     */
    size_t size() const { return updq.size(); }

    /**
     * @brief Returns the maximum number of elements.
     *
     * Returns the maximum allowed number of elements.
     *
     * @return  queue capacity (max. number of elements)
     */
    size_t capacity() const { return maxElements; }

private:
    size_t maxElements;
    bool discardOldest;
    epicsMutex lock;
    std::queue<std::shared_ptr<Update<T>>> updq;
};

} // namespace DevOpcua

#endif // DEVOPCUA_UPDATEQUEUE_H
