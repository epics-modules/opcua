/*************************************************************************\
* Copyright (c) 2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <gtest/gtest.h>

#include <epicsTypes.h>
#include <epicsThread.h>

#include "Session.h"
#include "SessionUaSdk.h"

namespace {

using namespace DevOpcua;

TEST(NamespaceMapTest, NoMapping) {
    epicsThreadOnce(&Session::onceId, &Session::initOnce, nullptr);
    SessionUaSdk *sess = new SessionUaSdk("test", "url");

    for (unsigned int i = 0; i <= USHRT_MAX; i++) {
        OpcUa_UInt16 ns = static_cast<OpcUa_UInt16>(i);
        EXPECT_EQ(ns, sess->mapNamespaceIndex(ns)) << "numerical index mapped without mapping table";
    }
}

TEST(NamespaceMapTest, UnusedMapping) {
    SessionUaSdk *sess = new SessionUaSdk("test", "url");

    sess->addNamespaceMapping(1u, "one");
    sess->addNamespaceMapping(2u, "two");

    for (unsigned int i = 0; i <= USHRT_MAX; i++) {
        OpcUa_UInt16 ns = static_cast<OpcUa_UInt16>(i);
        EXPECT_EQ(ns, sess->mapNamespaceIndex(ns)) << "numerical index changed without valid mapping";
    }
}

// More tests will need a semi-fuctional mock
// - to feed the namespace array into the table
// - to simulate server reboots with namespace array change

} // namespace
