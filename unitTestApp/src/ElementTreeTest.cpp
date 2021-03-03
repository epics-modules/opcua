/*************************************************************************\
* Copyright (c) 2021 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <gtest/gtest.h>
#include <memory>
#include <utility>

#include <epicsTime.h>

#include "ElementTree.h"
#include "linkParser.h"

namespace {

using namespace DevOpcua;

// Test class with the minimal required interface
// and test reference counting

class TestItem;

class TestNode
{
public:
    TestNode(const std::string &name, std::shared_ptr<TestNode> parent)
        : name(name)
        , parent(parent)
    {
        instanceCount++;
    }

    TestNode(const std::string &name, TestItem *item)
        : name(name)
        , item(item)
    {
        instanceCount++;
    }

    void
    addChild(std::weak_ptr<TestNode> elem)
    {
        elements.push_back(elem);
    }

    void
    setParent(std::shared_ptr<TestNode> elem)
    {
        parent = elem;
    }

    std::shared_ptr<TestNode>
    findChild(const std::string &name)
    {
        for (auto it : elements)
            if (auto pit = it.lock())
                if (pit->name == name)
                    return pit;
        return std::shared_ptr<TestNode>();
    }

    bool
    isLeaf()
    {
        return name[0] == 'l';
    }

    static unsigned int
    instances()
    {
        return instanceCount;
    }

    bool
    hasChild(std::shared_ptr<TestNode> &test)
    {
        bool found = false;
        for (auto it : elements) {
            auto el = it.lock();
            if (el && el.get() == test.get()) {
                found = true;
                break;
            }
        }
        return found;
    }

    ~TestNode() { instanceCount--; }
    const std::string name;
    std::vector<std::weak_ptr<TestNode>> elements;
private:
    std::shared_ptr<TestNode> parent;

    static unsigned int instanceCount;
    TestItem *item;
};

unsigned int TestNode::instanceCount = 0;

// Fixture
// r0 = empty
// r1 = [root] - n01 - n011
//                   + n012
// nx are shared pointers to keep the structure alive
class CreateStructureTest : public ::testing::Test
{
protected:
    CreateStructureTest()
        : r0(item)
        , r1(item)
    {
        n01 = std::make_shared<TestNode>("n01", nullptr);
        n011 = std::make_shared<TestNode>("n011", n01);
        n01->addChild(n011);
        n011->setParent(n01);
        n012 = std::make_shared<TestNode>("n012", n01);
        n01->addChild(n012);
        n012->setParent(n01);

        r1.addLeaf(n01, {"n01"});

        l0 = std::make_shared<TestNode>("l0", item);
        l1 = std::make_shared<TestNode>("l1", item);

        fixNodeInstances = TestNode::instances();
    }

    unsigned int
    addedNodes()
    {
        return TestNode::instances() - fixNodeInstances;
    }

    TestItem *item = nullptr;
    ElementTree<TestNode, TestItem> r0;
    ElementTree<TestNode, TestItem> r1;
    std::shared_ptr<TestNode> l0;
    std::shared_ptr<TestNode> l1;
    std::shared_ptr<TestNode> n01;
    std::shared_ptr<TestNode> n011;
    std::shared_ptr<TestNode> n012;
    unsigned int fixNodeInstances;
};

/* std::shared_ptr<E>
 * nearestNode(std::list<std::string> &path)
 *
 * @brief Find the existing part of an element path and return pointer to closest node.
 *
 * @param[in,out] path  element path; existing leading nodes will be removed
 *
 * @return shared_ptr to the closest existing node in the tree, shared_ptr<>() if no overlap
 */

TEST_F(CreateStructureTest, fixtureSetup_RefCountersOk)
{
    EXPECT_EQ(TestNode::instances(), 6u) << "fixture does not contain 6 TestNode instances";
}

TEST_F(CreateStructureTest, operatorBool_EmptyAndNonemptyTree)
{
    EXPECT_FALSE(bool(r0)) << "empty ElementTree does not return false";
    EXPECT_TRUE(bool(r1)) << "non-empty ElementTree does not return true";
}

TEST_F(CreateStructureTest, nearestNode_NodeLeafToEmptyRoot)
{
    auto path(splitString("n0.l0"));
    std::shared_ptr<TestNode> closest = r0.nearestNode(path);
    EXPECT_EQ(closest, r0.root().lock())
        << "checking node+leaf against empty root doesn't return an empty pointer";
    EXPECT_EQ(path.size(), 2u) << "checking node+leaf against empty root changes the path size";
}

TEST_F(CreateStructureTest, nearestNode_LeafToRootNode)
{
    auto path(splitString("l0"));
    std::shared_ptr<TestNode> closest = r1.nearestNode(path);
    EXPECT_EQ(closest, r1.root().lock())
        << "checking leaf against root node doesn't return the root itself";
    EXPECT_EQ(path.size(), 1u) << "checking leaf against root doesn't return a path of size 1";
    EXPECT_EQ(path.front(), "l0") << "checking leaf against root doesn't return the leaf in path";
}

TEST_F(CreateStructureTest, nearestNode_NodeLeafToRootNode)
{
    auto path(splitString("n02.l0"));
    std::shared_ptr<TestNode> closest = r1.nearestNode(path);
    EXPECT_EQ(closest, r1.root().lock())
        << "checking node+leaf against root node doesn't return the root itself";
    EXPECT_EQ(path.size(), 2u) << "checking node+leaf against root doesn't return a path of size 2";
    EXPECT_EQ(path.front(), "n02")
        << "checking node+leaf against root doesn't return node in path[0]";
    EXPECT_EQ(path.back(), "l0")
        << "checking node+leaf against root doesn't return leaf in path[1]";
}

TEST_F(CreateStructureTest, nearestNode_NodeLeafToSecondNode)
{
    auto path(splitString("n01.n013.l0"));
    std::shared_ptr<TestNode> closest = r1.nearestNode(path);
    EXPECT_EQ(closest, n01) << "checking node+leaf against 2nd node n01 doesn't return n01";
    EXPECT_EQ(path.size(), 2u)
        << "checking node+leaf against 2nd node doesn't return a path of size 2";
    EXPECT_EQ(path.front(), "n013")
        << "checking node+leaf against 2nd node doesn't return node in path[0]";
    EXPECT_EQ(path.back(), "l0")
        << "checking node+leaf against 2nd node doesn't return leaf in path[1]";
}

TEST_F(CreateStructureTest, nearestNode_NodeLeafToThirdNode)
{
    auto path(splitString("n01.n011.n0112.l0"));
    std::shared_ptr<TestNode> closest = r1.nearestNode(path);
    EXPECT_EQ(closest, n011) << "checking node+leaf against 3rd node n011 doesn't return n011";
    EXPECT_EQ(path.size(), 2u)
        << "checking node+leaf against 3rd node doesn't return a path of size 2";
    EXPECT_EQ(path.front(), "n0112")
        << "checking node+leaf against 3rd node doesn't return node in path[0]";
    EXPECT_EQ(path.back(), "l0")
        << "checking node+leaf against 3rd node doesn't return leaf in path[1]";
}

TEST_F(CreateStructureTest, nearestNode_2NodeLeafToEmptyRoot)
{
    auto path(splitString("n2.n21.l0"));
    std::shared_ptr<TestNode> empty;
    std::shared_ptr<TestNode> closest = r0.nearestNode(path);
    EXPECT_EQ(closest, empty)
        << "checking 2nodes+leaf against non-existing root doesn't return <null>";
    EXPECT_EQ(path.size(), 3u)
        << "checking 2nodes+leaf against non-existing root changes the path size";
    EXPECT_EQ(path.front(), "n2")
        << "checking 2nodes+leaf against non-existing root doesn't return 'n2' in path[0]";
    EXPECT_EQ(path.back(), "l0")
        << "checking 2nodes+leaf against non-existing root doesn't return leaf in path[2]";
}

/* void
 * addLeaf(std::shared_ptr<E> leaf, const std::list<std::string> &fullpath)
 *
 * @brief Add a new leaf element to the element tree.
 *
 * @param leaf  shared_ptr to the leaf element to insert
 * @param fullpath  full path (list of path elements) of the leaf
 */

TEST_F(CreateStructureTest, addLeaf_UnnamedLeafToEmptyRoot)
{
    r0.addLeaf(l0, {});
    EXPECT_EQ(addedNodes(), 0u) << "adding unnamed leaf to empty root creates additional nodes";
    EXPECT_EQ(r0.root().lock(), l0) << "adding unnamed leaf to empty root doesn't make leaf the root node";
}

TEST_F(CreateStructureTest, addLeaf_NamedLeafToEmptyRoot)
{
    r0.addLeaf(l0, {"l0"});
    EXPECT_EQ(addedNodes(), 1u) << "adding named leaf to empty root doesn't create 1 additional node";
    auto node = r0.root().lock();
    EXPECT_EQ(node->hasChild(l0), true)
        << "adding named leaf to empty root doesn't show leaf l0 as a child of the root node";
}

TEST_F(CreateStructureTest, addLeaf_NodeLeafToEmptyRoot)
{
    r0.addLeaf(l0, splitString("n0.l0"));
    EXPECT_EQ(addedNodes(), 2u)
        << "adding node+leaf to empty root doesn't create 2 intermediate nodes";
    auto node1 = r0.root().lock();
    auto node2 = node1->elements.back().lock();
    EXPECT_EQ(node2->hasChild(l0), true) << "adding node+leaf to empty root doesn't show "
                                            "leaf l0 as a child of the 2nd intermediate node "
                                         << node2->name;
}

TEST_F(CreateStructureTest, addLeaf_2NodeLeafToEmptyRoot)
{
    r0.addLeaf(l0, splitString("n0.n01.l0"));
    EXPECT_EQ(addedNodes(), 3u)
        << "adding 2nodes+leaf to empty root doesn't create 3 intermediate nodes";
    auto node1 = r0.root().lock();
    auto node2 = node1->elements.back().lock();
    auto node3 = node2->elements.back().lock();
    EXPECT_EQ(node3->hasChild(l0), true) << "adding 2nodes+leaf to empty root doesn't show "
                                            "leaf l0 as a child of the 3rd intermediate node "
                                         << node3->name;
}

TEST_F(CreateStructureTest, addLeaf_LeafToExistingRoot)
{
    r1.addLeaf(l0, splitString("l0"));
    EXPECT_EQ(addedNodes(), 0u) << "adding leaf to existing root creates intermediate nodes";
    auto node = r1.root().lock();
    EXPECT_EQ(node->hasChild(l0), true) << "adding leaf to existing root doesn't show "
                                           "leaf l0 as a child of the root node "
                                        << node->name;
}

TEST_F(CreateStructureTest, addLeaf_NodeLeafToExistingRoot)
{
    r1.addLeaf(l0, splitString("n02.l0"));
    EXPECT_EQ(addedNodes(), 1u)
        << "adding node+leaf to existing root doesn't create intermediate node";
    auto node1 = r1.root().lock();
    auto node2 = node1->elements.back().lock();
    EXPECT_EQ(node2->hasChild(l0), true) << "adding node+leaf to existing root doesn't show "
                                            "leaf l0 as the last child of the intermediate node "
                                         << node2->name;
}

TEST_F(CreateStructureTest, addLeaf_2NodeLeafToExistingRoot)
{
    r1.addLeaf(l0, splitString("n02.n021.l0"));
    EXPECT_EQ(addedNodes(), 2u)
        << "adding 2nodes+leaf to existing root doesn't create 2 intermediate nodes";
    auto node1 = r1.root().lock();
    auto node2 = node1->elements.back().lock();
    auto node3 = node2->elements.back().lock();
    EXPECT_EQ(node3->hasChild(l0), true)
        << "adding 2nodes+leaf to existing root doesn't show "
           "leaf l0 as the last child of the 3rd intermediate node "
        << node3->name;
}

TEST_F(CreateStructureTest, addLeaf_LeafToSecondNode)
{
    r1.addLeaf(l0, splitString("n01.l0"));
    EXPECT_EQ(addedNodes(), 0u) << "adding leaf to second node creates intermediate nodes";
    auto node1 = r1.root().lock();
    auto node2 = node1->elements.back().lock();
    EXPECT_EQ(node2->hasChild(l0), true) << "adding leaf to 2nd node doesn't show "
                                            "leaf l0 as the last child of the 2nd node "
                                         << node2->name;
}

TEST_F(CreateStructureTest, addLeaf_NodeLeafToSecondNode)
{
    r1.addLeaf(l0, splitString("n01.n013.l0"));
    EXPECT_EQ(addedNodes(), 1u)
        << "adding node+leaf to second node doesn't create intermediate node";
    auto node1 = r1.root().lock();
    auto node2 = node1->elements.back().lock();
    auto node3 = node2->elements.back().lock();
    EXPECT_EQ(node3->hasChild(l0), true) << "adding node+leaf to 2nd node doesn't show "
                                            "leaf l0 as the last child of the 2nd+new node "
                                         << node3->name;
}

TEST_F(CreateStructureTest, addLeaf_2NodeLeafToSecondNode)
{
    r1.addLeaf(l0, splitString("n01.n013.n0131.l0"));
    EXPECT_EQ(addedNodes(), 2u)
        << "adding 2nodes+leaf to 2nd node doesn't create 2 intermediate nodes";
    auto node1 = r1.root().lock();
    auto node2 = node1->elements.back().lock();
    auto node3 = node2->elements.back().lock();
    auto node4 = node3->elements.back().lock();
    EXPECT_EQ(node4->hasChild(l0), true) << "adding 2nodes+leaf to 2nd node doesn't show "
                                            "leaf l0 as the last child of the 2nd+2new node "
                                         << node4->name;
}

TEST_F(CreateStructureTest, addLeaf_LeafToThirdNode)
{
    r1.addLeaf(l0, splitString("n01.n011.l0"));
    EXPECT_EQ(addedNodes(), 0u) << "adding leaf to 3rd node creates intermediate nodes";
    auto node1 = r1.root().lock();
    auto node2 = node1->elements.back().lock();
    auto node3 = node2->elements.front().lock();
    EXPECT_EQ(node3->hasChild(l0), true) << "adding leaf to 3rd node doesn't show "
                                            "leaf l0 as the last child of the 3rd node "
                                         << node3->name;
}

TEST_F(CreateStructureTest, addLeaf_NodeLeafToThirdNode)
{
    r1.addLeaf(l0, splitString("n01.n011.n0112.l0"));
    EXPECT_EQ(addedNodes(), 1u)
        << "adding node+leaf to 3rd node doesn't create intermediate node";
    auto node1 = r1.root().lock();
    auto node2 = node1->elements.back().lock();
    auto node3 = node2->elements.front().lock();
    auto node4 = node3->elements.back().lock();
    EXPECT_EQ(node4->hasChild(l0), true) << "adding node+leaf to 3rd node doesn't show "
                                            "leaf l0 as the last child of the 3rd+new node "
                                         << node4->name;
}

TEST_F(CreateStructureTest, addLeaf_2NodeLeafToThirdNode)
{
    r1.addLeaf(l0, splitString("n01.n011.n0112.n01122.l0"));
    EXPECT_EQ(addedNodes(), 2u)
        << "adding 2nodes+leaf to third node doesn't create 2 intermediate nodes";
    auto node1 = r1.root().lock();
    auto node2 = node1->elements.back().lock();
    auto node3 = node2->elements.front().lock();
    auto node4 = node3->elements.back().lock();
    auto node5 = node4->elements.back().lock();
    EXPECT_EQ(node5->hasChild(l0), true) << "adding 2nodes+leaf to 3rd node doesn't show "
                                            "leaf l0 as the last child of the 3rd+2new node "
                                         << node5->name;
}

// Error conditions

TEST_F(CreateStructureTest, addLeaf_LeafUnderExistingLeaf_throws)
{
    r1.addLeaf(l0, splitString("n0.l0"));
    EXPECT_THROW(r1.addLeaf(l1, splitString("n0.l0.l1")), std::runtime_error)
        << "adding leaf under leaf didn't throw";
}

TEST_F(CreateStructureTest, addLeaf_NodeLeafUnderExistingLeaf_throws)
{
    r1.addLeaf(l0, splitString("n0.l0"));
    EXPECT_THROW(r1.addLeaf(l1, splitString("n0.l0.n1.l1")), std::runtime_error)
        << "adding node+leaf under leaf didn't throw";
}

TEST_F(CreateStructureTest, addLeaf_UnnamedLeafToExistingRoot_throws)
{
    EXPECT_THROW(r1.addLeaf(l0, {}), std::runtime_error)
        << "adding unnamed leaf to existing root didn't throw";
}

} // namespace
