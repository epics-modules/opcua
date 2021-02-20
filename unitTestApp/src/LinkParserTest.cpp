/*************************************************************************\
* Copyright (c) 2021 ITER Organization.
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

#include "linkParser.h"

namespace {

using namespace DevOpcua;

/* std::vector<std::string> splitString(const std::string &str,
 *                                      const char delim = defaultElementDelimiter);
 *
 * @brief Split configuration string along delimiters into a vector<string>.
 *
 * Delimiters at the beginning or end of the string or multiple delimiters in a row
 * generate empty vector elements.
 *
 * @param str  string to split
 * @param delim  token delimiter
 *
 * @return  tokens in order of appearance as vector<string>
 */

TEST(LinkParserTest, splitString_empty)
{
    const std::string s = "";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    EXPECT_EQ(path[0], "") << "path[0] not empty after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_justOneDelimiter)
{
    const std::string s = ".";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 2u) << "path doesn't have 2 elements after splitting '" << s << "'";
    EXPECT_EQ(path[0], "") << "path[0] not empty after splitting '" << s << "'";
    EXPECT_EQ(path[1], "") << "path[1] not empty after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_justTwoDelimiters)
{
    const std::string s = "..";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    EXPECT_EQ(path[0], "") << "path[0] not empty after splitting '" << s << "'";
    EXPECT_EQ(path[1], "") << "path[1] not empty after splitting '" << s << "'";
    EXPECT_EQ(path[2], "") << "path[2] not empty after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_oneElem) {
    const std::string s = "one";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    EXPECT_EQ(path[0], "one") << "path[0] not 'one' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_twoElem) {
    const std::string s = "one.two";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 2u) << "path doesn't have 2 elements after splitting '" << s << "'";
    EXPECT_EQ(path[0], "one") << "path[0] not 'one' after splitting '" << s << "'";
    EXPECT_EQ(path[1], "two") << "path[1] not 'two' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_threeElem) {
    const std::string s = "one.two.three";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    EXPECT_EQ(path[0], "one") << "path[0] not 'one' after splitting '" << s << "'";
    EXPECT_EQ(path[1], "two") << "path[1] not 'two' after splitting '" << s << "'";
    EXPECT_EQ(path[2], "three") << "path[2] not 'three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_escapedDelimiter) {
    const std::string s = R"(one\.two)";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    EXPECT_EQ(path[0], "one.two") << "path[0] not 'one.two' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_twoEscapedDelimiters) {
    const std::string s = R"(one\.two\.three)";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    EXPECT_EQ(path[0], "one.two.three") << "path[0] not 'one.two.three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_seriesOfEscapedDelimiters) {
    const std::string s = R"(one\.\.\.two\.\.three)";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    EXPECT_EQ(path[0], "one...two..three") << "path[0] not 'one...two..three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_seriesOfEscapedBackslashesAndDelimiters) {
    const std::string s = R"(one\.\.\\.two\.\.\three)";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 1u) << "path doesn't have 1 element after splitting '" << s << "'";
    EXPECT_EQ(path[0], "one..\\.two..\\three") << "path[0] not 'one..\\.two..\\three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_startsWithDelimiter) {
    const std::string s = ".two.three";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    EXPECT_EQ(path[0], "") << "path[0] not empty after splitting '" << s << "'";
    EXPECT_EQ(path[1], "two") << "path[1] not 'two' after splitting '" << s << "'";
    EXPECT_EQ(path[2], "three") << "path[2] not 'three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_startsWithEscapedDelimiter) {
    const std::string s = R"(\..two.three)";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    EXPECT_EQ(path[0], ".") << "path[0] not '.' after splitting '" << s << "'";
    EXPECT_EQ(path[1], "two") << "path[1] not 'two' after splitting '" << s << "'";
    EXPECT_EQ(path[2], "three") << "path[2] not 'three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_startsWithTwoDelimiters) {
    const std::string s = "..three";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    EXPECT_EQ(path[0], "") << "path[0] not empty after splitting '" << s << "'";
    EXPECT_EQ(path[1], "") << "path[1] not empty after splitting '" << s << "'";
    EXPECT_EQ(path[2], "three") << "path[2] not 'three' after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_endsWithDelimiter) {
    const std::string s = "one.two.";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    EXPECT_EQ(path[0], "one") << "path[0] not 'one' after splitting '" << s << "'";
    EXPECT_EQ(path[1], "two") << "path[1] not 'two' after splitting '" << s << "'";
    EXPECT_EQ(path[2], "") << "path[2] not empty after splitting '" << s << "'";
}

TEST(LinkParserTest, splitString_endsWithTwoDelimiters) {
    const std::string s = "one..";
    std::vector<std::string> path = splitString(s);
    EXPECT_EQ(path.size(), 3u) << "path doesn't have 3 elements after splitting '" << s << "'";
    EXPECT_EQ(path[0], "one") << "path[0] not 'one' after splitting '" << s << "'";
    EXPECT_EQ(path[1], "") << "path[1] not empty after splitting '" << s << "'";
    EXPECT_EQ(path[2], "") << "path[2] not empty after splitting '" << s << "'";
}

} // namespace
