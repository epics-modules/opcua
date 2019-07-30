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

#include <epicsMutex.h>

#include "devOpcua.h"

namespace DevOpcua {

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
 *
 * The template parameter T is expected to be an instance of the Update class,
 * i.e. it must provide the override(), getOverrides() and getType() methods.
 */
template<typename T>
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
    void pushUpdate(std::shared_ptr<T> update, bool *wasFirst = nullptr)
    {
        Guard G(lock);
        if (wasFirst) *wasFirst = false;
        if (updq.size() < maxElements) {
            if (wasFirst && updq.empty()) *wasFirst = true;
            updq.push(update);
        } else {
            if (discardOldest) {
                std::shared_ptr<T> drop = updq.front();
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
    std::shared_ptr<T> popUpdate(ProcessReason *nextReason = nullptr)
    {
        Guard G(lock);
        std::shared_ptr<T> drop = updq.front();
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
    std::queue<std::shared_ptr<T>> updq;
};

} // namespace DevOpcua

#endif // DEVOPCUA_UPDATEQUEUE_H
