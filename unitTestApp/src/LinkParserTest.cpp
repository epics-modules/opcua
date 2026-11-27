/*************************************************************************\
* Copyright (c) 2021 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <list>
#include <gtest/gtest.h>

#include <epicsTime.h>

#include "linkParser.h"

namespace {

using namespace DevOpcua;

/* std::list<std::string> splitString(const std::string &str,
 *                                    const char delim = defaultElementDelimiter);
 *
 * @brief Split configuration string along delimiters into a list<string>.
 *
 * Delimiters at the beginning or end of the string or multiple delimiters in a row
 * generate empty list elements.
 *
 * @param str  string to split
 * @param delim  token delimiter
 *
 * @return  tokens in order of appearance as list<string>
 */

TEST(LinkParserTest, splitString_empty)
{
    const std::string s = "";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "") << "path[0] not empty after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_justOneDelimiter)
{
    const std::string s = ".";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 2u) << "path doesn't have 2 elements after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "") << "path[0] not empty after splitting '" << s << "'";
    EXPECT_EQ(*it++, "") << "path[1] not empty after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_justTwoDelimiters)
{
    const std::string s = "..";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "") << "path[0] not empty after splitting '" << s << "'";
    EXPECT_EQ(*it++, "") << "path[1] not empty after splitting '" << s << "'";
    EXPECT_EQ(*it++, "") << "path[2] not empty after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_oneElem) {
    const std::string s = "one";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "one") << "path[0] not 'one' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_twoElem) {
    const std::string s = "one.two";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 2u) << "path doesn't have 2 elements after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "one") << "path[0] not 'one' after splitting '" << s << "'";
    EXPECT_EQ(*it++, "two") << "path[1] not 'two' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_threeElem) {
    const std::string s = "one.two.three";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "one") << "path[0] not 'one' after splitting '" << s << "'";
    EXPECT_EQ(*it++, "two") << "path[1] not 'two' after splitting '" << s << "'";
    EXPECT_EQ(*it++, "three") << "path[2] not 'three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_escapedDelimiter) {
    const std::string s = R"(one\.two)";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "one.two") << "path[0] not 'one.two' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_twoEscapedDelimiters) {
    const std::string s = R"(one\.two\.three)";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "one.two.three") << "path[0] not 'one.two.three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_seriesOfEscapedDelimiters) {
    const std::string s = R"(one\.\.\.two\.\.three)";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "one...two..three") << "path[0] not 'one...two..three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_seriesOfEscapedBackslashesAndDelimiters) {
    const std::string s = R"(one\.\.\\.two\.\.\three)";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "one..\\.two..\\three") << "path[0] not 'one..\\.two..\\three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_startsWithDelimiter) {
    const std::string s = ".two.three";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "") << "path[0] not empty after splitting '" << s << "'";
    EXPECT_EQ(*it++, "two") << "path[1] not 'two' after splitting '" << s << "'";
    EXPECT_EQ(*it++, "three") << "path[2] not 'three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_startsWithEscapedDelimiter) {
    const std::string s = R"(\..two.three)";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, ".") << "path[0] not '.' after splitting '" << s << "'";
    EXPECT_EQ(*it++, "two") << "path[1] not 'two' after splitting '" << s << "'";
    EXPECT_EQ(*it++, "three") << "path[2] not 'three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_startsWithTwoDelimiters) {
    const std::string s = "..three";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "") << "path[0] not empty after splitting '" << s << "'";
    EXPECT_EQ(*it++, "") << "path[1] not empty after splitting '" << s << "'";
    EXPECT_EQ(*it++, "three") << "path[2] not 'three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_endsWithDelimiter) {
    const std::string s = "one.two.";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "one") << "path[0] not 'one' after splitting '" << s << "'";
    EXPECT_EQ(*it++, "two") << "path[1] not 'two' after splitting '" << s << "'";
    EXPECT_EQ(*it++, "") << "path[2] not empty after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_endsWithTwoDelimiters) {
    const std::string s = "one..";
    auto path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    auto it = path.begin();
    EXPECT_EQ(*it++, "one") << "path[0] not 'one' after splitting '" << s << "'";
    EXPECT_EQ(*it++, "") << "path[1] not empty after splitting '" << s << "'";
    EXPECT_EQ(*it++, "") << "path[2] not empty after splitting '" << s << "'";
}

/* std::list<BrowsePathElement> parseBrowsePath(const std::string &str);
 *
 * @brief Parse a browse path string into a list of BrowsePathElements.
 */

TEST(LinkParserTest, parseBrowsePath_singleName) {
    auto path = parseBrowsePath("Temperature");
    ASSERT_EQ(path.size(), 1u);
    EXPECT_FALSE(path.front().hasNamespace);
    EXPECT_EQ(path.front().name, "Temperature");
}

TEST(LinkParserTest, parseBrowsePath_singleNameWithNamespace) {
    auto path = parseBrowsePath("3:Temperature");
    ASSERT_EQ(path.size(), 1u);
    EXPECT_TRUE(path.front().hasNamespace);
    EXPECT_EQ(path.front().namespaceIndex, 3u);
    EXPECT_EQ(path.front().name, "Temperature");
}

TEST(LinkParserTest, parseBrowsePath_multipleElements) {
    auto path = parseBrowsePath("/2:PLC/Data Block/4:Sim.Temp");
    ASSERT_EQ(path.size(), 3u);
    auto it = path.begin();
    EXPECT_TRUE(it->hasNamespace);
    EXPECT_EQ(it->namespaceIndex, 2u);
    EXPECT_EQ(it->name, "PLC");
    ++it;
    EXPECT_FALSE(it->hasNamespace);
    EXPECT_EQ(it->name, "Data Block");
    ++it;
    EXPECT_TRUE(it->hasNamespace);
    EXPECT_EQ(it->namespaceIndex, 4u);
    EXPECT_EQ(it->name, "Sim.Temp");
}

TEST(LinkParserTest, parseBrowsePath_colonNotNamespace) {
    auto path = parseBrowsePath("a1:b/:c/x:2:y");
    ASSERT_EQ(path.size(), 3u);
    auto it = path.begin();
    EXPECT_FALSE(it->hasNamespace);
    EXPECT_EQ(it->name, "a1:b");
    ++it;
    EXPECT_FALSE(it->hasNamespace);
    EXPECT_EQ(it->name, ":c");
    ++it;
    EXPECT_FALSE(it->hasNamespace);
    EXPECT_EQ(it->name, "x:2:y");
}

TEST(LinkParserTest, parseBrowsePath_escapes) {
    auto path = parseBrowsePath("2&:Foo/a&/b/&&c/1:&3:d");
    ASSERT_EQ(path.size(), 4u);
    auto it = path.begin();
    EXPECT_FALSE(it->hasNamespace);
    EXPECT_EQ(it->name, "2:Foo");
    ++it;
    EXPECT_EQ(it->name, "a/b");
    ++it;
    EXPECT_EQ(it->name, "&c");
    ++it;
    EXPECT_TRUE(it->hasNamespace);
    EXPECT_EQ(it->namespaceIndex, 1u);
    EXPECT_EQ(it->name, "3:d");
}

TEST(LinkParserTest, parseBrowsePath_errors) {
    EXPECT_THROW(parseBrowsePath(""), std::runtime_error);
    EXPECT_THROW(parseBrowsePath("/"), std::runtime_error);
    EXPECT_THROW(parseBrowsePath("a//b"), std::runtime_error);
    EXPECT_THROW(parseBrowsePath("a/"), std::runtime_error);
    EXPECT_THROW(parseBrowsePath("a&"), std::runtime_error);
    EXPECT_THROW(parseBrowsePath("2:"), std::runtime_error);
    EXPECT_THROW(parseBrowsePath("70000:a"), std::runtime_error);
}

} // namespace
