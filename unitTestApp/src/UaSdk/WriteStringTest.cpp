/*************************************************************************\
* Copyright (c) 2026 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

/*
 * Tests for writing strings into elements of type LocalizedText and QualifiedName
 * ("locale|text", "nsIndex|name"), without an OPC UA server.
 */

#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <uavariant.h>

#include <dbCommon.h>
#include <epicsThread.h>

#include "DataElementUaSdkLeaf.h"
#include "ItemUaSdk.h"
#include "RecordConnector.h"
#include "Session.h"
#include "SessionUaSdk.h"
#include "devOpcua.h"

namespace {

using namespace DevOpcua;

// A session and an item with a single (fake) record connected to its value
class WriteStringTest : public ::testing::Test
{
protected:
    void
    SetUp() override
    {
        epicsThreadOnce(&Session::onceId, &Session::initOnce, nullptr);
        const std::string name = std::string("ws_") + ::testing::UnitTest::GetInstance()->current_test_info()->name();
        new SessionUaSdk(name, "opc.tcp://localhost:4840"); // registers itself

        prec = static_cast<dbCommon *>(std::calloc(1, sizeof(dbCommon)));
        ASSERT_NE(nullptr, prec);
        strncpy(prec->name, name.c_str(), sizeof(prec->name) - 1);

        pcon = new RecordConnector(prec);
        std::unique_ptr<linkInfo> pinfo(new linkInfo);
        pinfo->session = name;
        pinfo->identifierString = name;
        pinfo->monitor = false;
        pinfo->isOutput = true;
        pinfo->linkedToItem = true;
        pcon->plinkinfo = std::move(pinfo);
        item = new ItemUaSdk(*pcon->plinkinfo);
        item->recConnector = pcon;
        pcon->plinkinfo->item = item;
        pcon->pitem = item;
        DataElement::addElementToTree(item, pcon, pcon->plinkinfo->elementPath);
        leaf = dynamic_cast<DataElementUaSdkLeaf *>(pcon->pdataelement.get());
        ASSERT_NE(nullptr, leaf);
        // State down: incoming data only sets the element's type and cached value (no record processing)
    }
    // The item, session and connector are intentionally not torn down (process exit cleans up)

    void
    setIncoming(const UaVariant &value)
    {
        leaf->setIncomingData(value, ProcessReason::incomingData);
    }

    // Write the first len characters of buf
    long
    write(const char *buf, epicsUInt32 len)
    {
        return pcon->writeScalar(buf, len);
    }

    dbCommon *prec = nullptr;
    RecordConnector *pcon = nullptr;
    ItemUaSdk *item = nullptr;
    DataElementUaSdkLeaf *leaf = nullptr;
};

UaVariant
localizedText(const char *locale, const char *text)
{
    UaVariant v;
    v.setLocalizedText(UaLocalizedText(locale, text));
    return v;
}

UaVariant
qualifiedName(OpcUa_UInt16 ns, const char *name)
{
    UaVariant v;
    v.setQualifiedName(UaQualifiedName(name, ns));
    return v;
}

// The buffers contain more characters than written (as the VAL field of a record),
// to detect reading beyond the written length

TEST_F(WriteStringTest, LocalizedTextWithLocale)
{
    setIncoming(localizedText("de", "alt"));
    const char buf[] = "en|HelloXYZ";
    ASSERT_EQ(0, write(buf, 8)); // "en|Hello"
    UaLocalizedText lt;
    ASSERT_EQ(static_cast<OpcUa_StatusCode>(OpcUa_Good), leaf->getOutgoingData().toLocalizedText(lt));
    EXPECT_STREQ("en", UaString(lt.locale()).toUtf8());
    EXPECT_STREQ("Hello", UaString(lt.text()).toUtf8());
}

TEST_F(WriteStringTest, LocalizedTextEmptyLocale)
{
    setIncoming(localizedText("de", "alt"));
    const char buf[] = "|HiXYZ";
    ASSERT_EQ(0, write(buf, 3)); // "|Hi"
    UaLocalizedText lt;
    ASSERT_EQ(static_cast<OpcUa_StatusCode>(OpcUa_Good), leaf->getOutgoingData().toLocalizedText(lt));
    EXPECT_STREQ("", UaString(lt.locale()).toUtf8());
    EXPECT_STREQ("Hi", UaString(lt.text()).toUtf8());
}

TEST_F(WriteStringTest, LocalizedTextKeepsLocale)
{
    setIncoming(localizedText("de", "alt"));
    const char buf[] = "HalloXYZ";
    ASSERT_EQ(0, write(buf, 5)); // "Hallo"
    UaLocalizedText lt;
    ASSERT_EQ(static_cast<OpcUa_StatusCode>(OpcUa_Good), leaf->getOutgoingData().toLocalizedText(lt));
    EXPECT_STREQ("de", UaString(lt.locale()).toUtf8());
    EXPECT_STREQ("Hallo", UaString(lt.text()).toUtf8());
}

TEST_F(WriteStringTest, QualifiedNameWithNamespace)
{
    setIncoming(qualifiedName(3, "old"));
    const char buf[] = "2|NameXYZ";
    ASSERT_EQ(0, write(buf, 6)); // "2|Name"
    UaQualifiedName qn;
    ASSERT_EQ(static_cast<OpcUa_StatusCode>(OpcUa_Good), leaf->getOutgoingData().toQualifiedName(qn));
    EXPECT_EQ(2, qn.namespaceIndex());
    EXPECT_STREQ("Name", qn.toString().toUtf8());
}

TEST_F(WriteStringTest, QualifiedNameKeepsNamespace)
{
    setIncoming(qualifiedName(3, "old"));
    const char buf[] = "NameXYZ";
    ASSERT_EQ(0, write(buf, 4)); // "Name"
    UaQualifiedName qn;
    ASSERT_EQ(static_cast<OpcUa_StatusCode>(OpcUa_Good), leaf->getOutgoingData().toQualifiedName(qn));
    EXPECT_EQ(3, qn.namespaceIndex());
    EXPECT_STREQ("Name", qn.toString().toUtf8());
}

} // namespace
