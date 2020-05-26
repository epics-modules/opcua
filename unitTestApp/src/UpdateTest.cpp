/*************************************************************************\
* Copyright (c) 2019 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <memory>
#include <utility>
#include <gtest/gtest.h>

#include <epicsTime.h>

#include "UpdateQueue.h"

namespace {

using namespace DevOpcua;

TEST(UpdateTest, UpdateConstructor_WithDataRef_DataCorrectAndManaged) {
    epicsTime ts;
    ts.getCurrent();

    Update<int> u0(ts, ProcessReason::incomingData, 1);
    EXPECT_EQ(0ul, u0.getOverrides()) << "New Update has override counter > 0";
    EXPECT_EQ(ts, u0.getTimeStamp()) << "New Update time stamp differs from the provided stamp";
    EXPECT_EQ(ProcessReason::incomingData, u0.getType()) << "New Update ProcessReason differs from the provided type";
    EXPECT_EQ(true, bool(u0)) << "New Update (created with Data Reference) does not have data (operator bool returns false)";

    int i = u0.getData();
    EXPECT_EQ(1, i) << "Data from new Update (" << i << ") differs from the provided data (1)";

    std::unique_ptr<int> pi(u0.releaseData());
    EXPECT_EQ(false, bool(u0)) << "After releaseData(), Update does have data (operator bool returns true)";
    EXPECT_EQ(true, bool(pi)) << "unique_ptr released from new Update does not have data";
    EXPECT_EQ(1, *pi) << "Data released from new Update (" << *pi << ") differs from the provided data (1)";
}

TEST(UpdateTest, UpdateConstructor_WithUniquePtr_DataCorrectAndManaged) {
    epicsTime ts;
    ts.getCurrent();

    std::unique_ptr<int> pin(new int(1));

    Update<int> u0(ts, ProcessReason::readComplete, std::move(pin));
    EXPECT_EQ(0ul, u0.getOverrides()) << "New Update has override counter > 0";
    EXPECT_EQ(ts, u0.getTimeStamp()) << "New Update time stamp differs from the provided stamp";
    EXPECT_EQ(ProcessReason::readComplete,u0.getType()) << "New Update ProcessReason differs from the provided type";
    EXPECT_EQ(true, bool(u0)) << "New Update (created with unique_ptr) does not have data (operator bool returns false)";

    int i = u0.getData();
    EXPECT_EQ(1, i) << "Data from new Update (" << i << ") differs from the provided data (1)";

    std::unique_ptr<int> pi(u0.releaseData());
    EXPECT_EQ(false, bool(u0)) << "After releaseData(), Update does have data (operator bool returns true)";
    EXPECT_EQ(true, bool(pi)) << "unique_ptr released from new Update does not have data";
    EXPECT_EQ(1, *pi) << "Data released from new Update (" << *pi << ") differs from the provided data (1)";
}

TEST(UpdateTest, UpdateConstructor_NoData_DataEmpty) {
    epicsTime ts;
    ts.getCurrent();

    Update<int> u0(ts, ProcessReason::connectionLoss);
    EXPECT_EQ(0ul, u0.getOverrides()) << "New Update has override counter > 0";
    EXPECT_EQ(ts, u0.getTimeStamp()) << "New Update time stamp differs from the provided stamp";
    EXPECT_EQ(ProcessReason::connectionLoss, u0.getType()) << "New Update ProcessReason differs from the provided type";
    EXPECT_EQ(false, bool(u0)) << "New Update (created without data) does have data (operator bool returns true)";

    std::unique_ptr<int> pi(u0.releaseData());
    EXPECT_EQ(false, bool(pi)) << "unique_ptr released from empty Update does have data (operator bool returns true)";
}

TEST(UpdateTest, override_2xWithUpdate_OverriddenDataCorrect) {
    int i;
    epicsTime ts0;
    ts0.getCurrent();
    Update<int> u0(ts0, ProcessReason::incomingData, 0);
    epicsTime ts1 = ts0 + 1.0;
    Update<int> u1(ts1, ProcessReason::writeComplete, 1);
    epicsTime ts2 = ts0 + 2.0;
    Update<int> u2(ts2, ProcessReason::readComplete, 2);

    u0.override(u1);
    EXPECT_EQ(1ul, u0.getOverrides()) << "Update override counter (" << u0.getOverrides() << ") expected 1";
    EXPECT_EQ(ts1, u0.getTimeStamp()) << "Update timestamp is not set from override";
    EXPECT_EQ(ProcessReason::writeComplete, u0.getType()) << "Update ProcessReason is not set from override";
    EXPECT_EQ(true, bool(u0)) << "Update (after override 1) does not have data (operator bool returns false)";
    i = u0.getData();
    EXPECT_EQ(1, i) << "Update data (" << i << ") differs from override data (1)";
    EXPECT_EQ(false, bool(u1)) << "Override Update still has data (operator bool returns true)";

    u0.override(u2);
    EXPECT_EQ(2ul, u0.getOverrides()) << "Update override counter (" << u0.getOverrides() << ") expected 2";
    EXPECT_EQ(ts2, u0.getTimeStamp()) << "Update timestamp is not set from override";
    EXPECT_EQ(ProcessReason::readComplete, u0.getType()) << "Update ProcessReason is not set from override";
    i = u0.getData();
    EXPECT_EQ(2, i) << "Update data (" << i << ") differs from override data (2)";
    EXPECT_EQ(false, bool(u2)) << "Override Update still has data (operator bool returns true)";
}

TEST(UpdateTest, override_2xWithCounter_OverriddenDataCorrect) {
    int i;
    epicsTime ts0;
    ts0.getCurrent();
    Update<int> u0(ts0, ProcessReason::incomingData, 1);

    u0.override(2);
    EXPECT_EQ(3ul, u0.getOverrides()) << "Update override counter (" << u0.getOverrides() << ") is not 3";
    EXPECT_EQ(ts0, u0.getTimeStamp()) << "Update timestamp changed";
    EXPECT_EQ(ProcessReason::incomingData, u0.getType()) << "Update ProcessReason changed";
    i = u0.getData();
    EXPECT_EQ(1, i) << "Update data (" << i << ") changed";

    u0.override(3);
    EXPECT_EQ(7ul, u0.getOverrides()) << "Update override counter (" << u0.getOverrides() << ") is not 7";
    EXPECT_EQ(ts0, u0.getTimeStamp()) << "Update timestamp changed";
    EXPECT_EQ(ProcessReason::incomingData, u0.getType()) << "Update ProcessReason changed";
    i = u0.getData();
    EXPECT_EQ(1, i) << "Update data (" << i << ") changed";
}

} // namespace
