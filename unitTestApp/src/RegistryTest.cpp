/*************************************************************************\
* Copyright (c) 2021 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <utility>
#include <gtest/gtest.h>

#include <epicsMutex.h>
#include <epicsGuard.h>

#include "Registry.h"

namespace {

using namespace DevOpcua;
using namespace testing;

class TestObject
{
public:
    TestObject(unsigned int val)
        : tag(val)
    {}
    unsigned int tag;
};

// Fixture for testing single Registry
class RegistryTest : public ::testing::Test
{
public:
    RegistryTest()
        : r0(nm)
        , t0(0)
        , t1(1)
        , t2(2)
    {}

protected:
    RegistryKeyNamespace nm;
    Registry<TestObject> r0;
    TestObject t0, t1, t2;
};

TEST_F(RegistryTest, insert_fill_ReturnCorrectSizes)
{
    EXPECT_EQ(r0.size(), 0u) << "empty registry has size != 0";
    r0.insert({"object0", &t0});
    EXPECT_EQ(r0.size(), 1u) << "registry with 1 obj has size != 1";
    r0.insert({"object1", &t1});
    EXPECT_EQ(r0.size(), 2u) << "registry with 2 obj has size != 2";
    r0.insert({"object2", &t2});
    EXPECT_EQ(r0.size(), 3u) << "registry with 3 obj has size != 3";
}

TEST_F(RegistryTest, insert_sameKey_FirstEntryIsRetained)
{
    long status;

    status = r0.insert({"foo", &t0});
    EXPECT_TRUE(status == 0) << "first insertion of 'foo' returned error";
    status = r0.insert({"foo", &t1});
    EXPECT_FALSE(status == 0) << "second insertion of 'foo' returned success";
    EXPECT_EQ(r0.find("foo"), &t0) << "object of existing key was overwritten";
}

TEST_F(RegistryTest, find_exists_EntryIsReturned)
{
    r0.insert({"foo", &t0});
    r0.insert({"bar", &t1});
    r0.insert({"boo", &t2});
    EXPECT_EQ(r0.find("foo"), &t0) << "searching 'foo' returns wrong object";
    EXPECT_EQ(r0.find("bar"), &t1) << "searching 'bar' returns wrong object";
    EXPECT_EQ(r0.find("boo"), &t2) << "searching 'boo' returns wrong object";
}

TEST_F(RegistryTest, find_doesntExist_nullptrIsReturned)
{
    EXPECT_EQ(r0.find("boo"), nullptr) << "searching 'boo' returns object";
    r0.insert({"foo", &t0});
    r0.insert({"bar", &t1});
    EXPECT_EQ(r0.find("boo"), nullptr) << "searching 'boo' returns object";
}

TEST_F(RegistryTest, contains_mixed_returnValuesCorrect)
{
    EXPECT_EQ(r0.contains("boo"), false) << "contains() is true for non-existing key";
    r0.insert({"foo", &t0});
    r0.insert({"bar", &t1});
    EXPECT_EQ(r0.contains("boo"), false) << "contains() is true for non-existing key";
    EXPECT_EQ(r0.contains("foo"), true) << "contains() is false for existing key 'foo'";
    EXPECT_EQ(r0.contains("bar"), true) << "contains() is false for existing key 'bar'";
}

// Fixture for testing two Registries plus RegistryKeyNamespace (meta namespace watcher)
class MultiRegistryTest : public ::testing::Test
{
public:
    MultiRegistryTest()
        : r0(nm)
        , r1(nm)
        , t0(0)
        , t1(1)
        , t2(2)
    {}

protected:
    RegistryKeyNamespace nm;
    Registry<TestObject> r0, r1;
    TestObject t0, t1, t2;
};

TEST_F(MultiRegistryTest, insert_keyInOtherReg_DontInsert)
{
    long status;
    status = r0.insert({"foo", &t0});
    EXPECT_TRUE(status == 0) << "insertion of 'foo' in registry 0 returned error";
    status = r1.insert({"foo", &t1});
    EXPECT_FALSE(status == 0) << "insertion of 'foo' in registry 1 returned success";
    status = r1.insert({"bar", &t1});
    EXPECT_TRUE(status == 0) << "insertion of 'bar' in registry 1 returned error";
    status = r0.insert({"bar", &t2});
    EXPECT_FALSE(status == 0) << "insertion of 'bar' in registry 0 returned success";
    EXPECT_EQ(r0.contains("foo"), true) << "registry 0 doesn't contain 'foo'";
    EXPECT_EQ(r0.contains("bar"), false) << "registry 0 contains 'bar'";
    EXPECT_EQ(r1.contains("bar"), true) << "registry 1 doesn't contain 'bar'";
    EXPECT_EQ(r1.contains("foo"), false) << "registry 1 contains 'foo'";
}

} // namespace
