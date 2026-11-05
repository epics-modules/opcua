/*************************************************************************\
* Copyright (c) 2026 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

/*
 * Creating open62541 custom data types from a (binary schema) type dictionary,
 * in particular the limits of open62541 type descriptions:
 * max. 255 members and max. 65535 bytes in-memory size (UA_DataType::memSize is 16 bit).
 */

#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <epicsThread.h>

// The classes under test are compiled into this test executable (not taken from
// the DLL): declare them like in the library sources, or MinGW fails to link
// the (otherwise dllimport) references to Session's static members
#define epicsExportSharedSymbols
#include "Session.h"
#include "SessionOpen62541.h"

#ifdef HAS_XMLPARSER

namespace DevOpcua {

// Test hook (friend of SessionOpen62541): parse a type dictionary, inspect the result
class SessionOpen62541TestAccess
{
public:
    static void
    addBinaryTypeId(SessionOpen62541 &session, const std::string &typeName, UA_UInt32 id, UA_UInt16 ns)
    {
        session.binaryTypeIds.emplace(typeName, UA_NODEID_NUMERIC(ns, id));
    }

    static void
    parse(SessionOpen62541 &session, const std::string &xml, UA_UInt16 ns)
    {
        xmlDocPtr doc = xmlReadMemory(xml.c_str(), static_cast<int>(xml.size()), nullptr, nullptr, 0);
        ASSERT_NE(nullptr, doc) << "invalid test dictionary";
        session.parseCustomDataTypes(xmlDocGetRootElement(doc), ns);
        xmlFreeDoc(doc);
    }

    static const UA_DataType *
    findType(const SessionOpen62541 &session, const std::string &typeName)
    {
        for (const auto &type : session.customTypes)
            if (typeName == type.typeName)
                return &type;
        return nullptr;
    }
};

} // namespace DevOpcua

namespace {

using namespace DevOpcua;

const UA_UInt16 ns = 2;

std::string
structType(const std::string &name, unsigned int members, const std::string &memberType)
{
    std::ostringstream xml;
    xml << "<opc:StructuredType Name=\"" << name << "\">";
    for (unsigned int i = 0; i < members; i++)
        xml << "<opc:Field Name=\"m" << i << "\" TypeName=\"" << memberType << "\"/>";
    xml << "</opc:StructuredType>";
    return xml.str();
}

class CustomTypeTest : public ::testing::Test
{
protected:
    void
    SetUp() override
    {
        epicsThreadOnce(&Session::onceId, &Session::initOnce, nullptr);
    }

    // Parse a dictionary with the given types (all types get a binaryEncodingId)
    SessionOpen62541 *
    parse(const std::string &sessionName, const std::vector<std::pair<std::string, std::string>> &types)
    {
        auto *session = new SessionOpen62541(sessionName, "opc.tcp://localhost:4840");
        std::string xml = "<opc:TypeDictionary xmlns:opc=\"http://opcfoundation.org/BinarySchema/\" "
                          "xmlns:tns=\"urn:test\" TargetNamespace=\"urn:test\">";
        UA_UInt32 id = 7000;
        for (const auto &type : types) {
            SessionOpen62541TestAccess::addBinaryTypeId(*session, type.first, id++, ns);
            xml += type.second;
        }
        xml += "</opc:TypeDictionary>";
        SessionOpen62541TestAccess::parse(*session, xml, ns);
        return session;
    }
};

TEST_F(CustomTypeTest, SmallStructure)
{
    auto *s = parse("typesSmall",
                    {{"Small",
                      "<opc:StructuredType Name=\"Small\">"
                      "<opc:Field Name=\"a\" TypeName=\"opc:Double\"/>"
                      "<opc:Field Name=\"b\" TypeName=\"opc:Int32\"/>"
                      "</opc:StructuredType>"}});
    const UA_DataType *t = SessionOpen62541TestAccess::findType(*s, "Small");
    ASSERT_NE(nullptr, t);
    EXPECT_EQ(2u, t->membersSize);
    EXPECT_EQ(16u, t->memSize) << "double + int32, padded to 8 byte alignment";
}

TEST_F(CustomTypeTest, ArrayAfterPaddedLengthField)
{
    // The Int32 length field needs 3 bytes padding after the Byte;
    // removing it must also remove that padding: b @0, Arr {size_t, ptr} @8 -> 24 bytes
    auto *s = parse("typesArray",
                    {{"WithArray",
                      "<opc:StructuredType Name=\"WithArray\">"
                      "<opc:Field Name=\"b\" TypeName=\"opc:Byte\"/>"
                      "<opc:Field Name=\"NoOfArr\" TypeName=\"opc:Int32\"/>"
                      "<opc:Field Name=\"Arr\" TypeName=\"opc:Double\" LengthField=\"NoOfArr\"/>"
                      "</opc:StructuredType>"}});
    const UA_DataType *t = SessionOpen62541TestAccess::findType(*s, "WithArray");
    ASSERT_NE(nullptr, t);
    ASSERT_EQ(2u, t->membersSize) << "length field not removed";
    EXPECT_TRUE(t->members[1].isArray);
    EXPECT_EQ(7u, t->members[1].padding) << "array not aligned after the Byte member";
    EXPECT_EQ(24u, t->memSize);
}

TEST_F(CustomTypeTest, ArrayWithoutLengthField)
{
    // An array as first field: its length field can not be the previous member
    auto *s = parse("typesNoLength",
                    {{"NoLength",
                      "<opc:StructuredType Name=\"NoLength\">"
                      "<opc:Field Name=\"Arr\" TypeName=\"opc:Double\" LengthField=\"NoOfArr\"/>"
                      "</opc:StructuredType>"}});
    EXPECT_EQ(nullptr, SessionOpen62541TestAccess::findType(*s, "NoLength"))
        << "type with an array without length field accepted";
}

TEST_F(CustomTypeTest, InMemorySizeLimit)
{
    // Inner: 200 doubles = 1600 bytes
    // Outer40: 64000 bytes (fits), Outer41: 65600 bytes (exceeds 65535)
    // UsesOuter41: has a member of the rejected type
    auto *s = parse("typesSize",
                    {{"Inner", structType("Inner", 200, "opc:Double")},
                     {"Outer40", structType("Outer40", 40, "tns:Inner")},
                     {"Outer41", structType("Outer41", 41, "tns:Inner")},
                     {"UsesOuter41",
                      "<opc:StructuredType Name=\"UsesOuter41\">"
                      "<opc:Field Name=\"big\" TypeName=\"tns:Outer41\"/>"
                      "</opc:StructuredType>"}});

    const UA_DataType *inner = SessionOpen62541TestAccess::findType(*s, "Inner");
    ASSERT_NE(nullptr, inner);
    EXPECT_EQ(1600u, inner->memSize);

    const UA_DataType *outer40 = SessionOpen62541TestAccess::findType(*s, "Outer40");
    ASSERT_NE(nullptr, outer40);
    EXPECT_EQ(64000u, outer40->memSize);

    EXPECT_EQ(nullptr, SessionOpen62541TestAccess::findType(*s, "Outer41"))
        << "type with 65600 bytes in-memory size accepted (memSize wraps around)";
    EXPECT_EQ(nullptr, SessionOpen62541TestAccess::findType(*s, "UsesOuter41"))
        << "type with a member of a rejected type accepted";
}

TEST_F(CustomTypeTest, MemberCountLimit)
{
    auto *s = parse("typesMembers",
                    {{"Wide255", structType("Wide255", 255, "opc:Int32")},
                     {"Wide256", structType("Wide256", 256, "opc:Int32")}});

    const UA_DataType *wide255 = SessionOpen62541TestAccess::findType(*s, "Wide255");
    ASSERT_NE(nullptr, wide255);
    EXPECT_EQ(255u, wide255->membersSize);
    EXPECT_EQ(255u * 4u, wide255->memSize);

    EXPECT_EQ(nullptr, SessionOpen62541TestAccess::findType(*s, "Wide256"))
        << "type with 256 members accepted";
}

} // namespace

#else // HAS_XMLPARSER

TEST(CustomTypeTest, NoXmlParser)
{
    GTEST_SKIP() << "open62541 custom types need the XML parser (OPEN62541_USE_XMLPARSER)";
}

#endif // HAS_XMLPARSER
