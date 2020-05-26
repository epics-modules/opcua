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
    EXPECT_EQ(0lu, q0.size()) << "Empty update queue returns size " << q0.size();
    EXPECT_EQ(true, q0.empty()) << "Empty update queue returns empty() as false";
}

TEST_F(UpdateQueueTest, status_UsedQueue_IsCorrect) {
    epicsTime ts0;
    ts0.getCurrent();
    std::shared_ptr<Update<int>> u0(new Update<int> (ts0, ProcessReason::incomingData, 0));
    epicsTime ts1 = ts0 + 1.0;
    std::shared_ptr<Update<int>> u1(new Update<int> (ts1, ProcessReason::writeComplete, 1));
    q0.pushUpdate(u0);
    q0.pushUpdate(u1);

    EXPECT_EQ(2lu, q0.size()) << "With two updates, update queue returns size " << q0.size() << " not 2";
    EXPECT_EQ(false, q0.empty()) << "With two updates, update queue returns empty() as true";
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

    EXPECT_EQ(3lu, q0.size()) << "With 3 updates, update queue returns size " << q0.size() << " not 3";

    std::shared_ptr<Update<int>> r0 = q0.popUpdate();
    EXPECT_EQ(2lu, q0.size()) << "With 2 updates remaining, update queue returns size " << q0.size() << " not 2";
    EXPECT_EQ(0ul, r0->getOverrides()) << "Update 0 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(ts0, r0->getTimeStamp()) << "Update 0 timestamp is not as before";
    EXPECT_EQ(ProcessReason::incomingData, r0->getType()) << "Update 0 ProcessReason is not as before";
    i = r0->getData();
    EXPECT_EQ(0, i) << "Update 0 data (" << i << ") differs from original data (0)";

    r0 = q0.popUpdate();
    EXPECT_EQ(1lu, q0.size()) << "With 1 update remaining, update queue returns size " << q0.size() << " not 1";
    EXPECT_EQ(0ul, r0->getOverrides()) << "Update 1 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(ts1, r0->getTimeStamp()) << "Update 1 timestamp is not as before";
    EXPECT_EQ(ProcessReason::writeComplete, r0->getType()) << "Update 1 ProcessReason is not as before";
    i = r0->getData();
    EXPECT_EQ(1, i) << "Update 1 data (" << i << ") differs from original data (1)";

    r0 = q0.popUpdate();
    EXPECT_EQ(0lu, q0.size()) << "With 0 updates remaining, update queue returns size " << q0.size() << " not 0";
    EXPECT_EQ(0ul, r0->getOverrides()) << "Update 2 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(ts2, r0->getTimeStamp()) << "Update 2 timestamp is not as before";
    EXPECT_EQ(ProcessReason::readComplete, r0->getType()) << "Update 2 ProcessReason is not as before";
    i = r0->getData();
    EXPECT_EQ(2, i) << "Update 2 data (" << i << ") differs from original data (2)";
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
    EXPECT_EQ(2lu, q1.size()) << "After pop 1/3, update queue returns size " << q1.size() << " not 2";
    EXPECT_EQ(3ul, r0->getOverrides()) << "Pop 1/3 override counter (" << r0->getOverrides() << ") not 3";
    EXPECT_EQ(ts0, r0->getTimeStamp()) << "Pop 1/3 timestamp is not as from first added Update";

    r0 = q1.popUpdate();
    EXPECT_EQ(1lu, q1.size()) << "After pop 2/3, update queue returns size " << q1.size() << " not 1";
    EXPECT_EQ(0ul, r0->getOverrides()) << "Pop 2/3 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(ts1, r0->getTimeStamp()) << "Pop 2/3 timestamp is not as from 2nd added Update";

    r0 = q1.popUpdate();
    EXPECT_EQ(0lu, q1.size()) << "After pop 3/3, update queue returns size " << q1.size() << " not 0";
    EXPECT_EQ(0ul, r0->getOverrides()) << "Pop 3/3 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(ts2, r0->getTimeStamp()) << "Pop 3/3 timestamp is not as from 3rd added Update";
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
    EXPECT_EQ(2lu, q2.size()) << "After pop 1/3, update queue returns size " << q2.size() << " not 2";
    EXPECT_EQ(0ul, r0->getOverrides()) << "Pop 1/3 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(ts00, r0->getTimeStamp()) << "Pop 1/3 timestamp is not as from first original Update";

    r0 = q2.popUpdate();
    EXPECT_EQ(1lu, q2.size()) << "After pop 2/3, update queue returns size " << q2.size() << " not 1";
    EXPECT_EQ(0ul, r0->getOverrides()) << "Pop 2/3 override counter (" << r0->getOverrides() << ") not 0";
    EXPECT_EQ(ts01, r0->getTimeStamp()) << "Pop 2/3 timestamp is not as from 2nd original Update";

    r0 = q2.popUpdate();
    EXPECT_EQ(0lu, q2.size()) << "After pop 3/3, update queue returns size " << q2.size() << " not 0";
    EXPECT_EQ(3ul, r0->getOverrides()) << "Pop 3/3 override counter (" << r0->getOverrides() << ") not 3";
    EXPECT_EQ(ts2, r0->getTimeStamp()) << "Pop 3/3 timestamp is not as from 3rd added Update";
}

} // namespace
