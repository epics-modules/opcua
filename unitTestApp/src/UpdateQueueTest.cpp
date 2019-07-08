/*************************************************************************\
* Copyright (c) 2019 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <memory>
#include <gtest/gtest.h>

#include <epicsTime.h>

#include "UpdateQueue.h"

namespace {

using namespace DevOpcua;

// Fixture for testing UpdateQueue (empty, sizes 5 and 3, discard oldest)
class UpdateQueueTest : public ::testing::Test {
protected:
    UpdateQueueTest()
        : q0(5ul)
        , q1(3ul)
        , q2(3ul, false)
    {}

    virtual void SetUp() override {
        ts00.getCurrent();
        std::shared_ptr<Update<int>> u0(new Update<int> (ts00, ProcessReason::writeComplete, 0));
        ts01 = ts00 + 1.0;
        std::shared_ptr<Update<int>> u1(new Update<int> (ts01, ProcessReason::incomingData, 1));
        ts02 = ts00 + 2.0;
        std::shared_ptr<Update<int>> u2(new Update<int> (ts02, ProcessReason::readComplete, 2));

        q1.pushUpdate(u0);
        q1.pushUpdate(u1);
        q1.pushUpdate(u2);

        q2.pushUpdate(u0);
        q2.pushUpdate(u1);
        q2.pushUpdate(u2);
    }
    // virtual void TearDown() override {}
    epicsTime ts00;
    epicsTime ts01;
    epicsTime ts02;
    UpdateQueue<int> q0;
    UpdateQueue<int> q1;
    UpdateQueue<int> q2;
};

TEST_F(UpdateQueueTest, status_EmptyQueue_IsCorrect) {
    EXPECT_EQ(q0.size(), 0lu) << "Empty update queue returns size " << q0.size();
    EXPECT_EQ(q0.empty(), true) << "Empty update queue returns empty() as false";
    EXPECT_EQ(q0.capacity(), 5ul) << "Queue of size 5 reports " << q0.capacity() << " as capacity";
    EXPECT_EQ(q1.capacity(), 3ul) << "Queue of size 3 reports " << q0.capacity() << " as capacity";
}

TEST_F(UpdateQueueTest, status_UsedQueue_IsCorrect) {
    epicsTime ts0;
    ts0.getCurrent();
    std::shared_ptr<Update<int>> u0(new Update<int> (ts0, ProcessReason::incomingData, 0));
    epicsTime ts1 = ts0 + 1.0;
    std::shared_ptr<Update<int>> u1(new Update<int> (ts1, ProcessReason::writeComplete, 1));
    q0.pushUpdate(u0);
    q0.pushUpdate(u1);

    EXPECT_EQ(q0.size(), 2lu) << "With two updates, update queue returns size " << q0.size() << " not 2";
    EXPECT_EQ(q0.empty(), false) << "With two updates, update queue returns empty() as true";
}

TEST_F(UpdateQueueTest, popUpdate_UsedQueue_DataAndOrderCorrect) {
    int i;
    epicsTime ts0;
    ts0.getCurrent();
    std::shared_ptr<Update<int>> u0(new Update<int> (ts0, ProcessReason::incomingData, 0));
    epicsTime ts1 = ts0 + 1.0;
    std::shared_ptr<Update<int>> u1(new Update<int> (ts1, ProcessReason::writeComplete, 1));
    epicsTime ts2 = ts0 + 2.0;
    std::shared_ptr<Update<int>> u2(new Update<int> (ts2, ProcessReason::readComplete, 2));
    q0.pushUpdate(u0);
    q0.pushUpdate(u1);
    q0.pushUpdate(u2);

    EXPECT_EQ(q0.size(), 3lu) << "With 3 updates, update queue returns size " << q0.size() << " not 3";

    std::shared_ptr<Update<int>> r0 = q0.popUpdate();
    EXPECT_EQ(q0.size(), 2lu) << "With 2 updates remaining, update queue returns size " << q0.size() << " not 2";
    EXPECT_EQ(r0->getOverrides(), 0ul) << "Update 0 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(r0->getTimeStamp(), ts0) << "Update 0 timestamp is not as before";
    EXPECT_EQ(r0->getType(), ProcessReason::incomingData) << "Update 0 ProcessReason is not as before";
    i = r0->getData();
    EXPECT_EQ(i, 0) << "Update 0 data (" << i << ") differs from original data (0)";

    r0 = q0.popUpdate();
    EXPECT_EQ(q0.size(), 1lu) << "With 1 update remaining, update queue returns size " << q0.size() << " not 1";
    EXPECT_EQ(r0->getOverrides(), 0ul) << "Update 1 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(r0->getTimeStamp(), ts1) << "Update 1 timestamp is not as before";
    EXPECT_EQ(r0->getType(), ProcessReason::writeComplete) << "Update 1 ProcessReason is not as before";
    i = r0->getData();
    EXPECT_EQ(i, 1) << "Update 1 data (" << i << ") differs from original data (1)";

    r0 = q0.popUpdate();
    EXPECT_EQ(q0.size(), 0lu) << "With 0 updates remaining, update queue returns size " << q0.size() << " not 0";
    EXPECT_EQ(r0->getOverrides(), 0ul) << "Update 2 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(r0->getTimeStamp(), ts2) << "Update 2 timestamp is not as before";
    EXPECT_EQ(r0->getType(), ProcessReason::readComplete) << "Update 2 ProcessReason is not as before";
    i = r0->getData();
    EXPECT_EQ(i, 2) << "Update 2 data (" << i << ") differs from original data (2)";
}

TEST_F(UpdateQueueTest, popUpdate_UsedQueue_nextReasonIsCorrect) {
    epicsTime ts0;
    ts0.getCurrent();
    std::shared_ptr<Update<int>> u0(new Update<int> (ts0, ProcessReason::incomingData, 0));
    epicsTime ts1 = ts0 + 1.0;
    std::shared_ptr<Update<int>> u1(new Update<int> (ts1, ProcessReason::writeComplete, 1));
    ProcessReason nextReason = ProcessReason::none;
    std::shared_ptr<Update<int>> r0;

    q0.pushUpdate(u0);
    q0.pushUpdate(u1);

    r0 = q0.popUpdate(&nextReason);
    EXPECT_EQ(nextReason, ProcessReason::writeComplete) << "Second-to-last pop does not set nextReason = writeComplete";
    r0 = q0.popUpdate(&nextReason);
    EXPECT_EQ(nextReason, ProcessReason::none) << "Last pop does not set nextReason = none";
}

TEST_F(UpdateQueueTest, pushUpdate_FullQueueOldest_OverrideAtOldEnd) {
    std::shared_ptr<Update<int>> r0;
    epicsTime ts0;
    ts0.getCurrent();
    std::shared_ptr<Update<int>> u0(new Update<int> (ts0, ProcessReason::incomingData, 10));
    epicsTime ts1 = ts0 + 1.0;
    std::shared_ptr<Update<int>> u1(new Update<int> (ts1, ProcessReason::writeComplete, 11));
    epicsTime ts2 = ts0 + 2.0;
    std::shared_ptr<Update<int>> u2(new Update<int> (ts2, ProcessReason::readComplete, 12));
    q1.pushUpdate(u0);
    q1.pushUpdate(u1);
    q1.pushUpdate(u2);

    r0 = q1.popUpdate();
    EXPECT_EQ(q1.size(), 2lu) << "After pop 1/3, update queue returns size " << q1.size() << " not 2";
    EXPECT_EQ(r0->getOverrides(), 3ul) << "Pop 1/3 override counter (" << r0->getOverrides() << ") not 3";
    EXPECT_EQ(r0->getTimeStamp(), ts0) << "Pop 1/3 timestamp is not as from first added Update";

    r0 = q1.popUpdate();
    EXPECT_EQ(q1.size(), 1lu) << "After pop 2/3, update queue returns size " << q1.size() << " not 1";
    EXPECT_EQ(r0->getOverrides(), 0ul) << "Pop 2/3 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(r0->getTimeStamp(), ts1) << "Pop 2/3 timestamp is not as from 2nd added Update";

    r0 = q1.popUpdate();
    EXPECT_EQ(q1.size(), 0lu) << "After pop 3/3, update queue returns size " << q1.size() << " not 0";
    EXPECT_EQ(r0->getOverrides(), 0ul) << "Pop 3/3 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(r0->getTimeStamp(), ts2) << "Pop 3/3 timestamp is not as from 3rd added Update";
}

TEST_F(UpdateQueueTest, pushUpdate_FullQueueNewest_OverrideAtNewEnd) {
    std::shared_ptr<Update<int>> r0;
    epicsTime ts0;
    ts0.getCurrent();
    std::shared_ptr<Update<int>> u0(new Update<int> (ts0, ProcessReason::incomingData, 10));
    epicsTime ts1 = ts0 + 1.0;
    std::shared_ptr<Update<int>> u1(new Update<int> (ts1, ProcessReason::writeComplete, 11));
    epicsTime ts2 = ts0 + 2.0;
    std::shared_ptr<Update<int>> u2(new Update<int> (ts2, ProcessReason::readComplete, 12));
    q2.pushUpdate(u0);
    q2.pushUpdate(u1);
    q2.pushUpdate(u2);

    r0 = q2.popUpdate();
    EXPECT_EQ(q2.size(), 2lu) << "After pop 1/3, update queue returns size " << q2.size() << " not 2";
    EXPECT_EQ(r0->getOverrides(), 0ul) << "Pop 1/3 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(r0->getTimeStamp(), ts00) << "Pop 1/3 timestamp is not as from first original Update";

    r0 = q2.popUpdate();
    EXPECT_EQ(q2.size(), 1lu) << "After pop 2/3, update queue returns size " << q2.size() << " not 1";
    EXPECT_EQ(r0->getOverrides(), 0ul) << "Pop 2/3 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(r0->getTimeStamp(), ts01) << "Pop 2/3 timestamp is not as from 2nd original Update";

    r0 = q2.popUpdate();
    EXPECT_EQ(q2.size(), 0lu) << "After pop 3/3, update queue returns size " << q2.size() << " not 0";
    EXPECT_EQ(r0->getOverrides(), 3ul) << "Pop 3/3 override counter (" << r0->getOverrides() << ") not 3";
    EXPECT_EQ(r0->getTimeStamp(), ts2) << "Pop 3/3 timestamp is not as from 3rd added Update";
}

TEST_F(UpdateQueueTest, pushUpdate_EmptyQueue_wasFirstIsCorrect) {
    epicsTime ts0;
    ts0.getCurrent();
    std::shared_ptr<Update<int>> u0(new Update<int> (ts0, ProcessReason::incomingData, 0));
    epicsTime ts1 = ts0 + 1.0;
    std::shared_ptr<Update<int>> u1(new Update<int> (ts1, ProcessReason::writeComplete, 1));
    bool wasFirst = false;

    q0.pushUpdate(u0, &wasFirst);
    EXPECT_EQ(wasFirst, true) << "First push does not set wasFirst = true";
    q0.pushUpdate(u1, &wasFirst);
    EXPECT_EQ(wasFirst, false) << "Second push does not set wasFirst = false";
}

} // namespace
