/*************************************************************************\
* Copyright (c) 2019-2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <gtest/gtest.h>

#include <epicsTypes.h>

#include "DataElementUaSdkLeaf.h"

namespace {

using namespace DevOpcua;

TEST(RangeCheckTest, ToSByte) {

    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt8>(0))) << "SByte<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt8>(127))) << "SByte<-UInt8: 127 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt8>(128))) << "SByte<-UInt8: 128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt8>(255))) << "SByte<-UInt8: MAX (255) not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt8>(-128))) << "SByte<-Int8: -128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt8>(0))) << "SByte<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt8>(127))) << "SByte<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt16>(0))) << "SByte<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt16>(127))) << "SByte<-UInt16: 127 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt16>(128))) << "SByte<-UInt16: 128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt16>(65535))) << "SByte<-UInt16: MAX (2^16-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt16>(-32768))) << "SByte<-Int16: -32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt16>(-129))) << "SByte<-Int16: -129 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt16>(-128))) << "SByte<-Int16: -128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt16>(0))) << "SByte<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt16>(127))) << "SByte<-Int16: 127 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt16>(128))) << "SByte<-Int16: 128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt16>(32767))) << "SByte<-Int16: 32767 not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt32>(0))) << "SByte<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt32>(127))) << "SByte<-UInt32: 127 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt32>(128))) << "SByte<-UInt32: 128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt32>(4294967295))) << "SByte<-UInt32: MAX (2^32-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt32>(-2147483648ll))) << "SByte<-Int32: MIN (-2^31) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt32>(-129))) << "SByte<-Int32: -129 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt32>(-128))) << "SByte<-Int32: -128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt32>(0))) << "SByte<-Int32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt32>(127))) << "SByte<-Int32: 127 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt32>(128))) << "SByte<-Int32: 128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt32>(2147483647))) << "SByte<-Int32: MAX (2^31-1) not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt64>(0))) << "SByte<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt64>(127))) << "SByte<-UInt64: 127 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt64>(128))) << "SByte<-UInt64: 128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsUInt64>(18446744073709551615ull))) << "SByte<-UInt64: MAX (2^64-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "SByte<-Int64: MIN (-2^63) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt64>(-129))) << "SByte<-Int64: -129 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt64>(-128))) << "SByte<-Int64: -128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt64>(0))) << "SByte<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt64>(127))) << "SByte<-Int64: 127 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt64>(128))) << "SByte<-Int64: 128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsInt64>(9223372036854775807ll))) << "SByte<-Int64: MAX (2^63-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat32>(-FLT_MAX))) << "SByte<-Float32: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat32>(-129.0))) << "SByte<-Float32: -129. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat32>(-128.0))) << "SByte<-Float32: -128. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat32>(-0.))) << "SByte<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat32>(0.))) << "SByte<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat32>(127.))) << "SByte<-Float32: 127. detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat32>(128.))) << "SByte<-Float32: 128. not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat32>(FLT_MAX))) << "SByte<-Float32: MAX (large positive) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat64>(-DBL_MAX))) << "SByte<-Float64: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat64>(-129.0))) << "SByte<-Float64: -129. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat64>(-128.0))) << "SByte<-Float64: -128. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat64>(-0.))) << "SByte<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat64>(0.))) << "SByte<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat64>(127.))) << "SByte<-Float64: 127. detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat64>(128.))) << "SByte<-Float64: 128. not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_SByte>(static_cast<epicsFloat64>(DBL_MAX))) << "SByte<-Float64: MAX (large positive) not detected as out of range";
}

TEST(RangeCheckTest, ToByte) {

    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt8>(0))) << "Byte<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt8>(128))) << "Byte<-UInt8: 128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt8>(255))) << "Byte<-UInt8: MAX (255) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt8>(-128))) << "Byte<-Int8: -128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt8>(-1))) << "Byte<-Int8: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt8>(0))) << "Byte<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt8>(127))) << "Byte<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt16>(0))) << "Byte<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt16>(255))) << "Byte<-UInt16: 255 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt16>(256))) << "Byte<-UInt16: 256 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt16>(65535))) << "Byte<-UInt16: MAX (2^16-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt16>(-32768))) << "Byte<-Int16: -32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt16>(-1))) << "Byte<-Int16: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt16>(0))) << "Byte<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt16>(255))) << "Byte<-Int16: 255 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt16>(256))) << "Byte<-Int16: 256 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt16>(32767))) << "Byte<-Int16: 32767 not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt32>(0))) << "Byte<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt32>(255))) << "Byte<-UInt32: 255 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt32>(256))) << "Byte<-UInt32: 256 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt32>(4294967295))) << "Byte<-UInt32: MAX (2^32-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt32>(-2147483648ll))) << "Byte<-Int32: MIN (-2^31) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt32>(-1))) << "Byte<-Int32: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt32>(0))) << "Byte<-Int32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt32>(255))) << "Byte<-Int32: 255 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt32>(256))) << "Byte<-Int32: 256 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt32>(2147483647))) << "Byte<-Int32: MAX (2^31-1) not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt64>(0))) << "Byte<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt64>(255))) << "Byte<-UInt64: 255 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt64>(256))) << "Byte<-UInt64: 256 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsUInt64>(18446744073709551615ull))) << "Byte<-UInt64: MAX (2^64-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "Byte<-Int64: MIN (-2^63) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt64>(-1))) << "Byte<-Int64: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt64>(0))) << "Byte<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt64>(255))) << "Byte<-Int64: 255 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt64>(256))) << "Byte<-Int64: 256 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsInt64>(9223372036854775807ll))) << "Byte<-Int64: MAX (2^63-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat32>(-FLT_MAX))) << "Byte<-Float32: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat32>(-1.0))) << "Byte<-Float32: -1. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat32>(-0.))) << "Byte<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat32>(0.))) << "Byte<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat32>(255.))) << "Byte<-Float32: 255. detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat32>(256.))) << "Byte<-Float32: 256. not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat32>(FLT_MAX))) << "Byte<-Float32: MAX (large positive) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat64>(-DBL_MAX))) << "Byte<-Float64: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat64>(-1.0))) << "Byte<-Float64: -1. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat64>(-0.))) << "Byte<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat64>(0.))) << "Byte<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat64>(255.))) << "Byte<-Float64: 255. detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat64>(256.))) << "Byte<-Float64: 256. not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Byte>(static_cast<epicsFloat64>(DBL_MAX))) << "Byte<-Float64: MAX (large positive) not detected as out of range";
}

TEST(RangeCheckTest, ToInt16) {

    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt8>(0))) << "Int16<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt8>(128))) << "Int16<-UInt8: 128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt8>(255))) << "Int16<-UInt8: MAX (255) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt8>(-128))) << "Int16<-Int8: -128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt8>(0))) << "Int16<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt8>(127))) << "Int16<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt16>(0))) << "Int16<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt16>(32767))) << "Int16<-UInt16: 32767 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt16>(32768))) << "Int16<-UInt16: 32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt16>(65535))) << "Int16<-UInt16: MAX (2^16-1) not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt16>(-32768))) << "Int16<-Int16: -32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt16>(0))) << "Int16<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt16>(32767))) << "Int16<-Int16: 32767 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt32>(0))) << "Int16<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt32>(32767))) << "Int16<-UInt32: 32767 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt32>(32768))) << "Int16<-UInt32: 32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt32>(4294967295))) << "Int16<-UInt32: MAX (2^32-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt32>(-2147483648ll))) << "Int16<-Int32: MIN (-2^31) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt32>(-32769))) << "Int16<-Int32: -32769 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt32>(-32768))) << "Int16<-Int32: -32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt32>(0))) << "Int16<-Int32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt32>(32767))) << "Int16<-Int32: 32767 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt32>(32768))) << "Int16<-Int32: 32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt32>(2147483647))) << "Int16<-Int32: MAX (2^31-1) not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt64>(0))) << "Int16<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt64>(32767))) << "Int16<-UInt64: 32767 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt64>(32768))) << "Int16<-UInt64: 32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsUInt64>(18446744073709551615ull))) << "Int16<-UInt64: MAX (2^64-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "Int16<-Int64: MIN (-2^63) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt64>(-32769))) << "Int16<-Int64: -32769 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt64>(-32768))) << "Int16<-Int64: -32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt64>(0))) << "Int16<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt64>(32767))) << "Int16<-Int64: 32767 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt64>(32768))) << "Int16<-Int64: 32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsInt64>(9223372036854775807ll))) << "Int16<-Int64: MAX (2^63-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat32>(-FLT_MAX))) << "Int16<-Float32: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat32>(-32769.0))) << "Int16<-Float32: -32769. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat32>(-32768.0))) << "Int16<-Float32: -32768. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat32>(-0.))) << "Int16<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat32>(0.))) << "Int16<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat32>(32767.))) << "Int16<-Float32: 32767. detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat32>(32768.))) << "Int16<-Float32: 32768. not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat32>(FLT_MAX))) << "Int16<-Float32: MAX (large positive) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat64>(-DBL_MAX))) << "Int16<-Float64: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat64>(-32769.0))) << "Int16<-Float64: -32769. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat64>(-32768.0))) << "Int16<-Float64: -32768. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat64>(-0.))) << "Int16<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat64>(0.))) << "Int16<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat64>(32767.))) << "Int16<-Float64: 32767. detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat64>(32768.))) << "Int16<-Float64: 32768. not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int16>(static_cast<epicsFloat64>(DBL_MAX))) << "Int16<-Float64: MAX (large positive) not detected as out of range";
}

TEST(RangeCheckTest, ToUInt16) {

    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt8>(0))) << "UInt16<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt8>(128))) << "UInt16<-UInt8: 128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt8>(255))) << "UInt16<-UInt8: MAX (255) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt8>(-128))) << "UInt16<-Int8: -128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt8>(-1))) << "UInt16<-Int8: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt8>(0))) << "UInt16<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt8>(127))) << "UInt16<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt16>(0))) << "UInt16<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt16>(32768))) << "UInt16<-UInt16: 32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt16>(65535))) << "UInt16<-UInt16: MAX (2^16-1) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt16>(-32768))) << "UInt16<-Int16: -32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt16>(-1))) << "UInt16<-Int16: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt16>(0))) << "UInt16<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt16>(32767))) << "UInt16<-Int16: 32767 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt32>(0))) << "UInt16<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt32>(65535))) << "UInt16<-UInt32: 65535 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt32>(65536))) << "UInt16<-UInt32: 65536 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt32>(4294967295))) << "UInt16<-UInt32: MAX (2^32-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt32>(-2147483648ll))) << "UInt16<-Int32: MIN (-2^31) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt32>(-1))) << "UInt16<-Int32: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt32>(0))) << "UInt16<-Int32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt32>(65535))) << "UInt16<-Int32: 65535 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt32>(65536))) << "UInt16<-Int32: 65536 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt32>(2147483647))) << "UInt16<-Int32: MAX (2^31-1) not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt64>(0))) << "UInt16<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt64>(65535))) << "UInt16<-UInt64: 65535 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt64>(65536))) << "UInt16<-UInt64: 65536 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsUInt64>(18446744073709551615ull))) << "UInt16<-UInt64: MAX (2^64-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "UInt16<-Int64: MIN (-2^63) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt64>(-1))) << "UInt16<-Int64: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt64>(0))) << "UInt16<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt64>(65535))) << "UInt16<-Int64: 65535 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt64>(65536))) << "UInt16<-Int64: 65536 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsInt64>(9223372036854775807ll))) << "UInt16<-Int64: MAX (2^63-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat32>(-FLT_MAX))) << "UInt16<-Float32: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat32>(-1.0))) << "UInt16<-Float32: -1. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat32>(-0.))) << "UInt16<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat32>(0.))) << "UInt16<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat32>(65535.))) << "UInt16<-Float32: 65535. detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat32>(65536.))) << "UInt16<-Float32: 65536. not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat32>(FLT_MAX))) << "UInt16<-Float32: MAX (large positive) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat64>(-DBL_MAX))) << "UInt16<-Float64: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat64>(-1.0))) << "UInt16<-Float64: -1. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat64>(-0.))) << "UInt16<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat64>(0.))) << "UInt16<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat64>(65535.))) << "UInt16<-Float64: 65535. detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat64>(65536))) << "UInt16<-Float64: 65536. not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt16>(static_cast<epicsFloat64>(DBL_MAX))) << "UInt16<-Float64: MAX (large positive) not detected as out of range";
}

TEST(RangeCheckTest, ToInt32) {

    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt8>(0))) << "Int32<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt8>(128))) << "Int32<-UInt8: 128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt8>(255))) << "Int32<-UInt8: MAX (255) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt8>(-128))) << "Int32<-Int8: -128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt8>(0))) << "Int32<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt8>(127))) << "Int32<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt16>(0))) << "Int32<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt16>(32768))) << "Int32<-UInt16: 32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt16>(65535))) << "Int32<-UInt16: MAX (2^16-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt16>(-32768))) << "Int32<-Int16: -32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt16>(0))) << "Int32<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt16>(32767))) << "Int32<-Int16: 32767 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt32>(0))) << "Int32<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt32>(2147483647))) << "Int32<-UInt32: 2^31-1 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt32>(2147483648))) << "Int32<-UInt32: 2^31 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt32>(4294967295))) << "Int32<-UInt32: MAX (2^32-1) not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt32>(-2147483648ll))) << "Int32<-Int32: MIN (-2^31) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt32>(0))) << "Int32<-Int32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt32>(2147483647))) << "Int32<-Int32: MAX (2^31-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt64>(0))) << "Int32<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt64>(2147483647))) << "Int32<-UInt64: 2^31-1 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt64>(2147483648))) << "Int32<-UInt64: 2^31 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsUInt64>(18446744073709551615ull))) << "Int32<-UInt64: MAX (2^64-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "Int32<-Int64: MIN (-2^63) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt64>(-2147483649ll))) << "Int32<-Int64: -2^31-1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt64>(-2147483648ll))) << "Int32<-Int64: -2^31 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt64>(0))) << "Int32<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt64>(2147483647))) << "Int32<-Int64: 2^31-1 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt64>(2147483648))) << "Int32<-Int64: 2^31 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsInt64>(9223372036854775807ll))) << "Int32<-Int64: MAX (2^63-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat32>(-FLT_MAX))) << "Int32<-Float32: -MAX (large negative) not detected as out of range";
    // Need a larger offset because of the limited resolution
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat32>(-2147484000.))) << "Int32<-Float32: -2^31-x not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat32>(-2147483648.))) << "Int32<-Float32: -2^31 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat32>(-0.))) << "Int32<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat32>(0.))) << "Int32<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat32>(2147483647.))) << "Int32<-Float32: 2^31-1 detected as out of range";
    // Need a larger offset because of the limited resolution
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat32>(2147484000.))) << "Int32<-Float32: 2^31+x not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat32>(FLT_MAX))) << "Int32<-Float32: MAX (large positive) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat64>(-DBL_MAX))) << "Int32<-Float64: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat64>(-2147483649.))) << "Int32<-Float64: -2^31-1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat64>(-2147483648.))) << "Int32<-Float64: -2^31 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat64>(-0.))) << "Int32<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat64>(0.))) << "Int32<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat64>(2147483647.))) << "Int32<-Float64: 2^31-1 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat64>(2147483648.))) << "Int32<-Float64: 2^31 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int32>(static_cast<epicsFloat64>(DBL_MAX))) << "Int32<-Float64: MAX (large positive) not detected as out of range";
}

TEST(RangeCheckTest, ToUInt32) {

    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt8>(0))) << "UInt32<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt8>(128))) << "UInt32<-UInt8: 128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt8>(255))) << "UInt32<-UInt8: MAX (255) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt8>(-128))) << "UInt32<-Int8: -128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt8>(-1))) << "UInt32<-Int8: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt8>(0))) << "UInt32<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt8>(127))) << "UInt32<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt16>(0))) << "UInt32<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt16>(32768))) << "UInt32<-UInt16: 32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt16>(65535))) << "UInt32<-UInt16: MAX (2^16-1) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt16>(-32768))) << "UInt32<-Int16: -32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt16>(-1))) << "UInt32<-Int16: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt16>(0))) << "UInt32<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt16>(32767))) << "UInt32<-Int16: 32767 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt32>(0))) << "UInt32<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt32>(65536))) << "UInt32<-UInt32: 65536 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt32>(4294967295))) << "UInt32<-UInt32: MAX (2^32-1) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt32>(-2147483648ll))) << "UInt32<-Int32: MIN (-2^31) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt32>(-1))) << "UInt32<-Int32: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt32>(0))) << "UInt32<-Int32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt32>(65536))) << "UInt32<-Int32: 65536 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt32>(2147483647))) << "UInt32<-Int32: MAX (2^31-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt64>(0))) << "UInt32<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt64>(4294967295))) << "UInt32<-UInt64: 2^32-1 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt64>(4294967296))) << "UInt32<-UInt64: 2^32 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsUInt64>(18446744073709551615ull))) << "UInt32<-UInt64: MAX (2^64-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "UInt32<-Int64: MIN (-2^63) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt64>(-1))) << "UInt32<-Int64: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt64>(0))) << "UInt32<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt64>(4294967295))) << "UInt32<-Int64: 2^32-1 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt64>(4294967296))) << "UInt32<-Int64: 2^32 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsInt64>(9223372036854775807ll))) << "UInt32<-Int64: MAX (2^63-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat32>(-FLT_MAX))) << "UInt32<-Float32: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat32>(-1.0))) << "UInt32<-Float32: -1. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat32>(-0.))) << "UInt32<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat32>(0.))) << "UInt32<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat32>(4294967295.))) << "UInt32<-Float32: 2^32-1 detected as out of range";
    // Need a larger offset because of the limited resolution
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat32>(4294967700.))) << "UInt32<-Float32: 2^32+x not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat32>(FLT_MAX))) << "UInt32<-Float32: MAX (large positive) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat64>(-DBL_MAX))) << "UInt32<-Float64: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat64>(-1.0))) << "UInt32<-Float64: -1. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat64>(-0.))) << "UInt32<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat64>(0.))) << "UInt32<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat64>(4294967295.))) << "UInt32<-Float64: 2^32-1 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat64>(4294967296.))) << "UInt32<-Float64: 2^32 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt32>(static_cast<epicsFloat64>(DBL_MAX))) << "UInt32<-Float64: MAX (large positive) not detected as out of range";
}

TEST(RangeCheckTest, ToInt64) {

    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt8>(0))) << "Int64<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt8>(128))) << "Int64<-UInt8: 128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt8>(255))) << "Int64<-UInt8: MAX (255) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt8>(-128))) << "Int64<-Int8: -128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt8>(0))) << "Int64<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt8>(127))) << "Int64<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt16>(0))) << "Int64<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt16>(32768))) << "Int64<-UInt16: 32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt16>(65535))) << "Int64<-UInt16: MAX (2^16-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt16>(-32768))) << "Int64<-Int16: -32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt16>(0))) << "Int64<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt16>(32767))) << "Int64<-Int16: 32767 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt32>(0))) << "Int64<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt32>(2147483648))) << "Int64<-UInt32: 2^31 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt32>(4294967295))) << "Int64<-UInt32: MAX (2^32-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt64>(-2147483648ll))) << "Int64<-Int64: MIN (-2^31) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt64>(0))) << "Int64<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt64>(2147483647))) << "Int64<-Int64: MAX (2^31-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt64>(0))) << "Int64<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt64>(9223372036854775807ull))) << "Int64<-UInt64: 2^63-1 detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt64>(9223372036854775808ull))) << "Int64<-UInt64: 2^63 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsUInt64>(18446744073709551615ull))) << "Int64<-UInt64: MAX (2^64-1) not detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "Int64<-Int64: MIN (-2^63) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt64>(0))) << "Int64<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsInt64>(9223372036854775807ll))) << "Int64<-Int64: MAX (2^63-1) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat32>(-FLT_MAX))) << "Int64<-Float32: -MAX (large negative) not detected as out of range";
    // Need a larger offset because of the limited resolution
    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat32>(-9223372600000000000.))) << "Int64<-Float32: -2^63-x not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat32>(-9223372036854775808.))) << "Int64<-Float32: -2^63 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat32>(-0.))) << "Int64<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat32>(0.))) << "Int64<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat32>(9223372036854775807.))) << "Int64<-Float32: 2^63-1 detected as out of range";
    // Need a larger offset because of the limited resolution
    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat32>(9223372600000000000.))) << "Int64<-Float32: 2^63+x not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat32>(FLT_MAX))) << "Int64<-Float32: MAX (large positive) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat64>(-DBL_MAX))) << "Int64<-Float64: -MAX (large negative) not detected as out of range";
    // Need a larger offset because of the limited resolution
    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat64>(-9223372036854777000.))) << "Int64<-Float64: -2^63-x not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat64>(-9223372036854775808.))) << "Int64<-Float64: -2^63 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat64>(-0.))) << "Int64<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat64>(0.))) << "Int64<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat64>(9223372036854775807.))) << "Int64<-Float64: 2^63-1 detected as out of range";
    // Need a larger offset because of the limited resolution
    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat64>(9223372036854777000.))) << "Int64<-Float64: 2^63+x not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Int64>(static_cast<epicsFloat64>(DBL_MAX))) << "Int64<-Float64: MAX (large positive) not detected as out of range";
}

TEST(RangeCheckTest, ToUInt64) {

    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt8>(0))) << "UInt64<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt8>(128))) << "UInt64<-UInt8: 128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt8>(255))) << "UInt64<-UInt8: MAX (255) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt8>(-128))) << "UInt64<-Int8: -128 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt8>(-1))) << "UInt64<-Int8: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt8>(0))) << "UInt64<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt8>(127))) << "UInt64<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt16>(0))) << "UInt64<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt16>(32768))) << "UInt64<-UInt16: 32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt16>(65535))) << "UInt64<-UInt16: MAX (2^16-1) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt16>(-32768))) << "UInt64<-Int16: -32768 not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt16>(-1))) << "UInt64<-Int16: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt16>(0))) << "UInt64<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt16>(32767))) << "UInt64<-Int16: 32767 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt32>(0))) << "UInt64<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt32>(65536))) << "UInt64<-UInt32: 65536 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt32>(4294967295))) << "UInt64<-UInt32: MAX (2^32-1) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt32>(-2147483648ll))) << "UInt64<-Int32: MIN (-2^31) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt32>(-1))) << "UInt64<-Int32: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt32>(0))) << "UInt64<-Int32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt32>(2147483647))) << "UInt64<-Int32: MAX (2^31-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt64>(0))) << "UInt64<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt64>(9223372036854775808ull))) << "UInt64<-UInt64: 2^63 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsUInt64>(18446744073709551615ull))) << "UInt64<-UInt64: MAX (2^64-1) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "UInt64<-Int64: MIN (-2^63) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt64>(-1))) << "UInt64<-Int64: -1 not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt64>(0))) << "UInt64<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsInt64>(9223372036854775807ll))) << "UInt64<-Int64: MAX (2^63-1) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat32>(-FLT_MAX))) << "UInt64<-Float32: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat32>(-1.0))) << "UInt64<-Float32: -1. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat32>(-0.))) << "UInt64<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat32>(0.))) << "UInt64<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat32>(18446744073709551615.))) << "UInt64<-Float32: 2^64-1 detected as out of range";
    // Need a larger offset because of the limited resolution
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat32>(18446746000000000000.))) << "UInt64<-Float32: 2^64+x not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat32>(FLT_MAX))) << "UInt64<-Float32: MAX (large positive) not detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat64>(-DBL_MAX))) << "UInt64<-Float64: -MAX (large negative) not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat64>(-1.0))) << "UInt64<-Float64: -1. not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat64>(-0.))) << "UInt64<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat64>(0.))) << "UInt64<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat64>(18446744073709551615.))) << "UInt64<-Float64: 2^64-1 detected as out of range";
    // Need a larger offset because of the limited resolution
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat64>(18446744073709555000.))) << "UInt64<-Float64: 2^64+x not detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_UInt64>(static_cast<epicsFloat64>(DBL_MAX))) << "UInt64<-Float64: MAX (large positive) not detected as out of range";
}

TEST(RangeCheckTest, ToFloat) {

    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt8>(0))) << "Float<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt8>(128))) << "Float<-UInt8: 128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt8>(255))) << "Float<-UInt8: MAX (255) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt8>(-128))) << "Float<-Int8: -128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt8>(0))) << "Float<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt8>(127))) << "Float<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt16>(0))) << "Float<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt16>(32768))) << "Float<-UInt16: 32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt16>(65535))) << "Float<-UInt16: MAX (2^16-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt16>(-32768))) << "Float<-Int16: -32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt16>(0))) << "Float<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt16>(32767))) << "Float<-Int16: 32767 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt32>(0))) << "Float<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt32>(2147483648))) << "Float<-UInt32: 2^31 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt32>(4294967295))) << "Float<-UInt32: MAX (2^32-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt64>(-2147483648ll))) << "Float<-Int64: MIN (-2^31) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt64>(0))) << "Float<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt64>(2147483647))) << "Float<-Int64: MAX (2^31-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt64>(0))) << "Float<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt64>(9223372036854775808ull))) << "Float<-UInt64: 2^63 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsUInt64>(18446744073709551615ull))) << "Float<-UInt64: MAX (2^64-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "Float<-Int64: MIN (-2^63) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt64>(0))) << "Float<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsInt64>(9223372036854775807ll))) << "Float<-Int64: MAX (2^63-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat32>(-FLT_MAX))) << "Float<-Float32: -MAX (large negative) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat32>(-0.))) << "Float<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat32>(0.))) << "Float<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat32>(FLT_MAX))) << "Float<-Float32: MAX (large positive) detected as out of range";

    EXPECT_FALSE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat64>(-DBL_MAX))) << "Float<-Float64: -DBL_MAX (large negative) not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat64>(-FLT_MAX))) << "Float<-Float64: -FLT_MAX (large negative) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat64>(-0.))) << "Float<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat64>(0.))) << "Float<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat64>(FLT_MAX))) << "Float<-Float64: FLT_MAX (large positive) detected as out of range";
    EXPECT_FALSE(isWithinRange<OpcUa_Float>(static_cast<epicsFloat64>(DBL_MAX))) << "Float<-Float64: DBL_MAX (large positive) not detected as out of range";
}

TEST(RangeCheckTest, ToDouble) {

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt8>(0))) << "Double<-UInt8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt8>(128))) << "Double<-UInt8: 128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt8>(255))) << "Double<-UInt8: MAX (255) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt8>(-128))) << "Double<-Int8: -128 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt8>(0))) << "Double<-Int8: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt8>(127))) << "Double<-Int8: 127 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt16>(0))) << "Double<-UInt16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt16>(32768))) << "Double<-UInt16: 32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt16>(65535))) << "Double<-UInt16: MAX (2^16-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt16>(-32768))) << "Double<-Int16: -32768 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt16>(0))) << "Double<-Int16: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt16>(32767))) << "Double<-Int16: 32767 detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt32>(0))) << "Double<-UInt32: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt32>(2147483648))) << "Double<-UInt32: 2^31 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt32>(4294967295))) << "Double<-UInt32: MAX (2^32-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt64>(-2147483648ll))) << "Double<-Int64: MIN (-2^31) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt64>(0))) << "Double<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt64>(2147483647))) << "Double<-Int64: MAX (2^31-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt64>(0))) << "Double<-UInt64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt64>(9223372036854775808ull))) << "Double<-UInt64: 2^63 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsUInt64>(18446744073709551615ull))) << "Double<-UInt64: MAX (2^64-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt64>(-9223372036854775807ll-1))) << "Double<-Int64: MIN (-2^63) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt64>(0))) << "Double<-Int64: 0 detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsInt64>(9223372036854775807ll))) << "Double<-Int64: MAX (2^63-1) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat32>(-FLT_MAX))) << "Double<-Float32: -MAX (large negative) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat32>(-0.))) << "Double<-Float32: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat32>(0.))) << "Double<-Float32: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat32>(FLT_MAX))) << "Double<-Float32: MAX (large positive) detected as out of range";

    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat64>(-DBL_MAX))) << "Double<-Float64: -DBL_MAX (large negative) not detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat64>(-FLT_MAX))) << "Double<-Float64: -FLT_MAX (large negative) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat64>(-0.))) << "Double<-Float64: -0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat64>(0.))) << "Double<-Float64: 0. detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat64>(FLT_MAX))) << "Double<-Float64: FLT_MAX (large positive) detected as out of range";
    EXPECT_TRUE(isWithinRange<OpcUa_Double>(static_cast<epicsFloat64>(DBL_MAX))) << "Double<-Float64: DBL_MAX (large positive) not detected as out of range";
}

} // namespace
