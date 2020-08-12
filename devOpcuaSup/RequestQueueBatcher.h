/*************************************************************************\
* Copyright (c) 2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#ifndef DEVOPCUA_REQUESTQUEUEBATCHER_H
#define DEVOPCUA_REQUESTQUEUEBATCHER_H

#include <memory>
#include <queue>
#include <vector>
#include <iostream>

#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <menuPriority.h>

#include "devOpcua.h"

namespace DevOpcua {

/**
 * @brief A queue + batcher for handling outgoing service requests.
 *
 * Items put requests (reads or writes) on the queue,
 * specifying the EPICS priority.
 * (Internally a set of 3 queues is used to implement priority queueing.)
 *
 * A worker thread pops requests from the queue and collects them into
 * a batch (std::vector<>), honoring the configured limit of items per service
 * request.
 *
 * The template parameter T is the implementation specific request cargo class
 * (i.e., the class of the things to be queued).
 */


/**
 * @brief Callback API for delivery of the request batches.
 */
template<typename T>
class RequestConsumer
{
public:
    /**
     * @brief Process a batch of requests.
     *
     * Called from the batcher thread to deliver a batch of requests to the
     * consumer (lower level).
     *
     * The argument is a reference to a vector of shared_ptr to cargo.
     * I.e., the callee has no (shared) ownership of the requests, and the
     * validity of the batch elements is only guaranteed during the call.
     *
     * A consumer that needs to establish shared ownership needs to explicitly
     * copy elements.
     *
     * @param batch  vector of requests (shared_ptr to cargo)
     */
    virtual void processRequests(std::vector<std::shared_ptr<T>> &batch) = 0;
};

template<typename T>
class RequestQueueBatcher : public epicsThreadRunable
{
public:
    RequestQueueBatcher(const std::string &name,
                        RequestConsumer<T> &consumer,
                        const unsigned maxRequestsPerBatch = 0,
                        const bool startWorkerNow = true)
        : maxBatchSize(maxRequestsPerBatch)
        , worker(*this, name.c_str(),
                 epicsThreadGetStackSize(epicsThreadStackSmall),
                 epicsThreadPriorityMedium)
        , workToDo(epicsEventEmpty)
        , workerShutdown(false)
        , consumer(consumer)
    {
        if (startWorkerNow)
            startWorker();
    }

    ~RequestQueueBatcher()
    {
        workerShutdown = true;
        workToDo.signal();
        worker.exitWait();
    }

    /**
     * @brief Starts the worker thread.
     */
    void startWorker() { worker.start(); }

    /**
     * @brief Pushes a request to the appropriate queue.
     *
     * Pushes the cargo to the appropriate queue and signals the worker thread.
     *
     * @param cargo  shared_ptr to the request
     * @param priority  EPICS priority (0=low, 1=mid, 2=high)
     */
    void pushRequest(std::shared_ptr<T> cargo,
                     const menuPriority priority)
    {
        Guard G(lock[priority]);
        queue[priority].push(cargo);
        workToDo.signal();
    }

    /**
     * @brief Checks whether a queue is empty.
     *
     * Checks if the queue has no elements for the specified priority.
     *
     * @param priority  EPICS priority (0=low, 1=mid, 2=high)
     * @return  `true` if the queue is empty, `false` otherwise
     */
    bool empty(const menuPriority priority) const { return queue[priority].empty(); }

    /**
     * @brief Returns the number of elements in a queue.
     *
     * Returns the number of elements for the specified priority.
     *
     * @param priority  EPICS priority (0=low, 1=mid, 2=high)
     * @return  number of elements in the queue
     */
    size_t size(const menuPriority priority) const { return queue[priority].size(); }

    // epicsThreadRunable API
    // Worker thread body
    virtual void run () override {
        bool allDone = true;
        do {
            std::vector<std::shared_ptr<T>> batch;

            if (allDone) workToDo.wait();
            if (workerShutdown) break;

            // Plain priority queue algorithm (for the time being)
            allDone = true;
            for (int prio = menuPriority_NUM_CHOICES-1; prio >= menuPriorityLOW; prio--) {
                if (!maxBatchSize || batch.size() < maxBatchSize) {
                    Guard G(lock[prio]);
                    while (queue[prio].size() && (!maxBatchSize || batch.size() < maxBatchSize)) {
                        batch.emplace_back(std::move(queue[prio].front()));
                        queue[prio].pop();
                    }
                }
                if (!queue[prio].empty())
                    allDone = false;
            }

            if (!batch.empty())
                consumer.processRequests(batch);

        } while (true);
    }

private:
    epicsMutex lock[menuPriority_NUM_CHOICES];
    std::queue<std::shared_ptr<T>> queue[menuPriority_NUM_CHOICES];
    const unsigned maxBatchSize;
    epicsThread worker;
    epicsEvent workToDo;
    bool workerShutdown;
    RequestConsumer<T> &consumer;
};

} // namespace DevOpcua

#endif // DEVOPCUA_REQUESTQUEUEBATCHER_H
