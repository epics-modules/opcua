/*************************************************************************\
* Copyright (c) 2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <memory>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <epicsTime.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <menuPriority.h>

#include "devOpcua.h"
#include "RequestQueueBatcher.h"

#define TAG_FINISHED UINT_MAX

static unsigned int nextTimeAdd = 2;
static void *fixture;
static epicsEvent allPushesDone;
static double lastHoldOff = 0.0;

namespace {

using namespace DevOpcua;
using namespace testing;

class TestCargo {
public:
    TestCargo(unsigned int val)
        : tag(val) {}
    unsigned int tag;
};

class TestDumper : public RequestConsumer<TestCargo> {
public:
    TestDumper() {}
    virtual ~TestDumper() {}
    virtual void processRequests(std::vector<std::shared_ptr<TestCargo>> &batch) override;
    void reset() {
        noOfBatches = 0;
        batchSizes.clear();
        batchData.clear();
        nextTimeAdd = 2;
        finished.tryWait();
    }

    epicsEvent finished;
    unsigned int noOfBatches;
    std::vector<unsigned int> batchSizes;
    std::vector<std::pair<double, std::vector<unsigned int>>> batchData;
};

void
TestDumper::processRequests(std::vector<std::shared_ptr<TestCargo>> &batch)
{
    noOfBatches++;
    batchSizes.push_back(static_cast<unsigned int>(batch.size()));
    std::vector<unsigned int> data;
    bool done = false;

    for (const auto &p : batch) {
        if (p->tag == TAG_FINISHED) {
            done = true;
        } else {
            data.push_back(p->tag);
        }
    }
    batchData.push_back(std::make_pair(lastHoldOff, std::move(data)));
    if (done) {
        finished.signal();
    }
}

TestDumper dump;

// Fixture for testing RequestQueueBatcher queues only (no worker thread)
class RQBQueuePushOnlyTest : public ::testing::Test {
protected:
    RQBQueuePushOnlyTest()
        : b0("test batcher 0", dump, 0, 0, 0, false)
        , c0(std::make_shared<TestCargo>(0))
        , c1(std::make_shared<TestCargo>(1))
        , c2(std::make_shared<TestCargo>(2))
    {
        b0.pushRequest(c0, menuPriorityLOW);
        b0.pushRequest(c1, menuPriorityMEDIUM);
        b0.pushRequest(c2, menuPriorityHIGH);
    }

    RequestQueueBatcher<TestCargo> b0;
    std::shared_ptr<TestCargo> c0, c1, c2;
};

TEST_F(RQBQueuePushOnlyTest, oncePerQueue_SizesRefCountsCorrect) {
    EXPECT_EQ(b0.size(menuPriorityLOW), 1lu) << "Queue LOW of size 1 returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityMEDIUM), 1lu) << "Queue MEDIUM of size 1 returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityHIGH), 1lu) << "Queue HIGH of size 1 returns wrong size";
    EXPECT_EQ(c0.use_count(), 2l) << "c0 does not have the correct reference count";
    EXPECT_EQ(c1.use_count(), 2l) << "c1 does not have the correct reference count";
    EXPECT_EQ(c2.use_count(), 2l) << "c2 does not have the correct reference count";
}

TEST_F(RQBQueuePushOnlyTest, twicePerQueue_SizesRefCountsCorrect) {
    b0.pushRequest(c0, menuPriorityLOW);
    b0.pushRequest(c1, menuPriorityMEDIUM);
    b0.pushRequest(c2, menuPriorityHIGH);

    EXPECT_EQ(b0.size(menuPriorityLOW), 2lu) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityMEDIUM), 2lu) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityHIGH), 2lu) << "Queue[HIGH] returns wrong size";
    EXPECT_EQ(c0.use_count(), 3l) << "c0 does not have the correct reference count";
    EXPECT_EQ(c1.use_count(), 3l) << "c1 does not have the correct reference count";
    EXPECT_EQ(c2.use_count(), 3l) << "c2 does not have the correct reference count";
}

TEST_F(RQBQueuePushOnlyTest, twicePerQueue_ClearEmptiesQueues) {
    b0.pushRequest(c0, menuPriorityLOW);
    b0.pushRequest(c1, menuPriorityMEDIUM);
    b0.pushRequest(c2, menuPriorityHIGH);

    EXPECT_EQ(b0.size(menuPriorityLOW), 2lu) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityMEDIUM), 2lu) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityHIGH), 2lu) << "Queue[HIGH] returns wrong size";

    b0.clear();

    EXPECT_EQ(b0.size(menuPriorityLOW), 0lu) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityMEDIUM), 0lu) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityHIGH), 0lu) << "Queue[HIGH] returns wrong size";
}

const unsigned int minTimeout = 2;
const unsigned int maxTimeout = 80;
const unsigned int minTimeout2 = 3;
const unsigned int maxTimeout2 = 100;

static void myThreadSleep(double seconds);

// Fixture for testing the batcher (with worker thread)
class RQBBatcherTest : public ::testing::Test {
public:
    void addB10hRequests(const unsigned int no)
    {
        addRequests(b10h, menuPriorityLOW, no);
    }

protected:
    RQBBatcherTest()
        : nextTag{2000000, 1000000, 0} // so that any batch must end up sorted
        , b0("test batcher 0", dump, 0, 0, 0, false)
        , b10("test batcher 10", dump, 10, 0, 0, false)
        , b100("test batcher 100", dump, 100)
        , b1000("test batcher 1k", dump, 1000, 0, 0, false)
        , b10h("test batcher 10h", dump, 10, minTimeout * 1000, maxTimeout * 1000, true, myThreadSleep)
    {
        dump.reset();
        allSentCargo.clear();
        ::fixture = &fixture();
    }

    virtual void TearDown() override
    {
        // use_count() = 1 for elements of allCargo => no reference lost
        unsigned int wrongUseCount = 0;
        for ( const auto &p : allSentCargo ) {
            if (p.use_count() != 1) wrongUseCount++;
        }
        EXPECT_EQ(wrongUseCount, 0u) << "members of cargo have use_count() not 1 after finish";

        // Strict PQ means each batch is sorted HIGH - MEDIUM - LOW and in the order of the queues
        for ( const auto &log : dump.batchData ) {
            const auto &data = log.second;
            if (data.size() > 1) {
                for ( unsigned int i = 0; i < data.size()-1; i++ ) {
                    EXPECT_LT(data[i], data[i+1]) << "Requests inside a batch out of order";
                }
            }
        }
    }

    RQBBatcherTest &fixture() { return *this; }

    void addRequests(RequestQueueBatcher<TestCargo> &b, const menuPriority priority, const unsigned int no)
    {
        Guard G(lock);
        for (unsigned int i = 0; i < no; i++) {
            auto cargo = std::make_shared<TestCargo>(nextTag[priority]++);
            b.pushRequest(cargo, priority);
            allSentCargo.emplace_back(std::move(cargo));
        }
    }

    void addRequestVector(RequestQueueBatcher<TestCargo> &b, const menuPriority priority, const unsigned int no)
    {
        Guard G(lock);
        auto v = std::vector<std::shared_ptr<TestCargo>>(no);
        for (unsigned int i = 0; i < no; i++)
            v[i] = std::make_shared<TestCargo>(nextTag[priority]++);
        b.pushRequest(v, priority);
        for (unsigned int i = 0; i < no; i++)
            allSentCargo.emplace_back(std::move(v[i]));
    }

    class Adder : public epicsThreadRunable
    {
    public:
        Adder(RQBBatcherTest &fix, RequestQueueBatcher<TestCargo> &b, const unsigned int no)
            : no(no)
            , parent(fix)
            , b(b)
            , t(*this, "adder", epicsThreadGetStackSize(epicsThreadStackSmall), epicsThreadPriorityMedium)
        {
            // Seed random number generator
            srand (static_cast<unsigned int>(time(nullptr)));
            t.start();
        }

        ~Adder() override { t.exitWait(); }

        virtual void run () override {
            unsigned int added = 0;
            while (added < no) {
                unsigned int i = std::max<unsigned int>(rand() % 7, no - added);
                parent.addRequests(b, static_cast<menuPriority>(rand() % 3), i);
                added += i;
                epicsThreadSleep(0.02 + 0.001 * (rand() % 23)); // allow context switch
            }
            done.signal();
        }

        epicsEvent done;
    private:
        unsigned int no;
        RQBBatcherTest &parent;
        RequestQueueBatcher<TestCargo> &b;
        epicsThread t;
    };

    void pushFinish_waitForDump(RequestQueueBatcher<TestCargo> &b)
    {
        b.pushRequest(std::make_shared<TestCargo>(TAG_FINISHED), menuPriorityLOW);
        dump.finished.wait();
    }

    epicsMutex lock;
    unsigned int nextTag[menuPriority_NUM_CHOICES];
    RequestQueueBatcher<TestCargo> b0, b10, b100, b1000, b10h;
    std::vector<std::shared_ptr<TestCargo>> allSentCargo;
};

TEST_F(RQBBatcherTest, setAndReadbackParameters) {
    EXPECT_EQ(b10h.maxRequests(), 10u) << "initial max requests parameter wrong";
    EXPECT_EQ(b10h.minHoldOff(), minTimeout * 1000) << "initial min holdoff time parameter wrong";
    EXPECT_EQ(b10h.maxHoldOff(), maxTimeout * 1000) << "initial max holdoff time parameter wrong";

    b10h.setParams(12, minTimeout2 * 1000, maxTimeout2 * 1000);

    EXPECT_EQ(b10h.maxRequests(), 12u) << "max requests parameter wrong (after setParams)";
    EXPECT_EQ(b10h.minHoldOff(), minTimeout2 * 1000) << "min holdoff time parameter wrong (after setParams)";
    EXPECT_NEAR(b10h.maxHoldOff(), maxTimeout2 * 1000, 5) << "max holdoff time parameter wrong (after setParams)";
}

TEST_F(RQBBatcherTest, sizeUnlimited_90RequestsInOneBatch) {
    addRequests(b0, menuPriorityLOW, 15);
    addRequests(b0, menuPriorityMEDIUM, 15);
    addRequests(b0, menuPriorityLOW, 15);
    addRequests(b0, menuPriorityHIGH, 15);
    addRequests(b0, menuPriorityMEDIUM, 15);
    addRequests(b0, menuPriorityHIGH, 15);
    // push the finish marker
    b0.pushRequest(std::make_shared<TestCargo>(TAG_FINISHED), menuPriorityLOW);

    EXPECT_EQ(b0.size(menuPriorityLOW), 31u) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityMEDIUM), 30u) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b0.size(menuPriorityHIGH), 30u) << "Queue[HIGH] of returns wrong size";

    b0.startWorker();
    dump.finished.wait();
    b0.pushRequest(std::make_shared<TestCargo>(TAG_FINISHED), menuPriorityLOW);
    dump.finished.wait();

    EXPECT_TRUE(b0.empty(menuPriorityLOW)) << "Queue[LOW] not empty";
    EXPECT_TRUE(b0.empty(menuPriorityMEDIUM)) << "Queue[MEDIUM] not empty";
    EXPECT_TRUE(b0.empty(menuPriorityHIGH)) << "Queue[HIGH] not empty";

    EXPECT_EQ(allSentCargo.size(), 90u) << "Not all cargo sent";
    EXPECT_EQ(dump.noOfBatches, 2u) << "Cargo not processed in single (+1) batch";
    EXPECT_EQ(dump.batchSizes[0], 91u) << "Batch[0] did not contain all cargo";
}

TEST_F(RQBBatcherTest, size1k_900RequestsInOneBatch) {
    addRequests(b1000, menuPriorityLOW, 300);
    addRequests(b1000, menuPriorityMEDIUM, 300);
    addRequests(b1000, menuPriorityHIGH, 300);
    // push the finish marker
    b1000.pushRequest(std::make_shared<TestCargo>(TAG_FINISHED), menuPriorityLOW);

    EXPECT_EQ(b1000.size(menuPriorityLOW), 301u) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b1000.size(menuPriorityMEDIUM), 300u) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b1000.size(menuPriorityHIGH), 300u) << "Queue[HIGH] of returns wrong size";

    b1000.startWorker();
    dump.finished.wait();
    b1000.pushRequest(std::make_shared<TestCargo>(TAG_FINISHED), menuPriorityLOW);
    dump.finished.wait();

    EXPECT_TRUE(b1000.empty(menuPriorityLOW)) << "Queue[LOW] not empty";
    EXPECT_TRUE(b1000.empty(menuPriorityMEDIUM)) << "Queue[MEDIUM] not empty";
    EXPECT_TRUE(b1000.empty(menuPriorityHIGH)) << "Queue[HIGH] not empty";

    EXPECT_EQ(allSentCargo.size(), 900u) << "Not all cargo sent";
    EXPECT_EQ(dump.noOfBatches, 2u) << "Cargo not processed in single (+1) batch";
    EXPECT_EQ(dump.batchSizes[0], 901u) << "Batch[0] did not contain all cargo";
}

TEST_F(RQBBatcherTest, size10_90RequestsManyBatches) {
    addRequests(b10, menuPriorityLOW, 30);
    addRequests(b10, menuPriorityMEDIUM, 30);
    addRequests(b10, menuPriorityHIGH, 30);

    b10.startWorker();
    pushFinish_waitForDump(b10);

    EXPECT_EQ(b10.size(menuPriorityLOW), 0u) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b10.size(menuPriorityMEDIUM), 0u) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b10.size(menuPriorityHIGH), 0u) << "Queue[HIGH] of returns wrong size";

    EXPECT_EQ(allSentCargo.size(), 90u) << "Not all cargo sent";
    EXPECT_EQ(dump.noOfBatches, 10u) << "Cargo not processed in 10 batches";
    EXPECT_THAT(dump.batchSizes, Each(Le(10u))) << "Some batches are exceeding the size limit";
}

TEST_F(RQBBatcherTest, size10_3Vectors90RequestsManyBatches) {
    addRequestVector(b10, menuPriorityLOW, 30);
    addRequestVector(b10, menuPriorityMEDIUM, 30);
    addRequestVector(b10, menuPriorityHIGH, 30);

    b10.startWorker();
    pushFinish_waitForDump(b10);

    EXPECT_EQ(b10.size(menuPriorityLOW), 0u) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b10.size(menuPriorityMEDIUM), 0u) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b10.size(menuPriorityHIGH), 0u) << "Queue[HIGH] of returns wrong size";

    EXPECT_EQ(allSentCargo.size(), 90u) << "Not all cargo sent";
    EXPECT_EQ(dump.noOfBatches, 10u) << "Cargo not processed in 10 batches";
    EXPECT_THAT(dump.batchSizes, Each(Le(10u))) << "Some batches are exceeding the size limit";
}

TEST_F(RQBBatcherTest, size100_100kRequests4ThreadsManyBatches) {
    Adder a1(fixture(), b100, 25000);
    Adder a2(fixture(), b100, 25000);
    Adder a3(fixture(), b100, 25000);
    Adder a4(fixture(), b100, 25000);
    a1.done.wait();
    a2.done.wait();
    a3.done.wait();
    a4.done.wait();

    // b100 is auto-started
    pushFinish_waitForDump(b100);

    EXPECT_EQ(b100.size(menuPriorityLOW), 0lu) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b100.size(menuPriorityMEDIUM), 0lu) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b100.size(menuPriorityHIGH), 0lu) << "Queue[HIGH] of returns wrong size";

    EXPECT_EQ(allSentCargo.size(), 100000u) << "Not all cargo sent";
    EXPECT_GE(dump.noOfBatches, 1000u) << "Cargo processed < 1000 batches";
    EXPECT_THAT(dump.batchSizes, Each(Le(100u))) << "Some batches are exceeding the size limit";
}

TEST_F(RQBBatcherTest, size10HoldOff_20RequestsVaryingBatches) {

    addRequests(b10h, menuPriorityLOW, 1);
    allPushesDone.wait();

    // b10h is auto-started
    pushFinish_waitForDump(b10h);

    EXPECT_EQ(b10h.size(menuPriorityLOW), 0lu) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b10h.size(menuPriorityMEDIUM), 0lu) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b10h.size(menuPriorityHIGH), 0lu) << "Queue[HIGH] of returns wrong size";

    EXPECT_EQ(allSentCargo.size(), 55u) << "Not all cargo sent";
    EXPECT_EQ(dump.noOfBatches, 11u) << "Cargo processed != 11 batches";
    EXPECT_THAT(dump.batchSizes, Each(Le(10u))) << "Some batches are exceeding the size limit";
    for (unsigned int i = 0; i < dump.batchData.size(); i++ ) {
        if (i < dump.batchData.size() - 1) {
            EXPECT_EQ(dump.batchData[i+1].first,
                    minTimeout + (maxTimeout-minTimeout) / 10.0 * dump.batchData[i].second.size() )
                    << "Wrong timeout period after batch " << i
                    << " (size " << dump.batchData[i].second.size() << ")";
        }
    }
}

TEST_F(RQBBatcherTest, size10HoldOff_20RequestsVaryingBatchesAfterParamChange) {
    b10h.setParams(10, minTimeout2 * 1000, maxTimeout2 * 1000);

    addRequests(b10h, menuPriorityLOW, 1);
    allPushesDone.wait();

    // b10h is auto-started
    pushFinish_waitForDump(b10h);

    EXPECT_EQ(b10h.size(menuPriorityLOW), 0lu) << "Queue[LOW] returns wrong size";
    EXPECT_EQ(b10h.size(menuPriorityMEDIUM), 0lu) << "Queue[MEDIUM] returns wrong size";
    EXPECT_EQ(b10h.size(menuPriorityHIGH), 0lu) << "Queue[HIGH] of returns wrong size";

    EXPECT_EQ(allSentCargo.size(), 55u) << "Not all cargo sent";
    EXPECT_EQ(dump.noOfBatches, 11u) << "Cargo processed != 11 batches";
    EXPECT_THAT(dump.batchSizes, Each(Le(10u))) << "Some batches are exceeding the size limit";
    for (unsigned int i = 0; i < dump.batchData.size(); i++ ) {
        if (i < dump.batchData.size() - 1) {
            EXPECT_EQ(dump.batchData[i+1].first,
                    minTimeout2 + (maxTimeout2-minTimeout2) / 10.0 * dump.batchData[i].second.size() )
                    << "Wrong timeout period after batch " << i
                    << " (size " << dump.batchData[i].second.size() << ")";
        }
    }
}

// Replacing libCom's epicsThreadSleep();

void
myThreadSleep(double seconds)
{
    lastHoldOff = seconds;
    if (nextTimeAdd < 11) {
        static_cast<RQBBatcherTest *>(fixture)->addB10hRequests(nextTimeAdd);
    } else if (nextTimeAdd == 11) {
        allPushesDone.signal();
    }
    nextTimeAdd++;
}

} // namespace
