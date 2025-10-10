/*************************************************************************\
* Copyright (c) 2020-2025 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#ifndef DEVOPCUA_ELEMENTTREE_H
#define DEVOPCUA_ELEMENTTREE_H

#include <list>
#include <memory>
#include <string>

#include "devOpcua.h"

namespace DevOpcua {

/**
 * @brief Tree of DataElements representing structured OPC UA data.
 *
 * Principle of operation for the DataElement tree
 *
 * An Element can be a structural Node, having children elements, or a Leaf,
 * pointing to a RecordConnector.
 * Each Element has a single parent Node. (The root Element has no parent.)
 *
 * In the simple case (scalar data), the root element *is* the unnamed leaf.
 * In the structure case, there is always a root node named [ROOT], so that
 * e.g. all leafs with simple names are children of that root node.
 *
 * The tree implementation makes heavy use of C++ smart pointers:
 * Each Element has a shared_ptr to its parent.
 * Each Node has a std::vector of weak_ptr to its children.
 * That way any Leaf can freely be added or removed and all intermediate
 * Nodes are properly added, ref-counted and deleted.
 *
 * The template types used (all specific to the implementation) and their
 * constraints:
 *
 * NE is the Node Element class
 *    with a constructor   NE::NE(const std::string &name, I *item)
 *    and methods          void NE::setParent(std::shared_ptr<NE> parent);
 *                         void NE::addChild(std::weak_ptr<NE> child);
 *                         std::shared_ptr<NE> findChild(const std::string &name)
 *
 * E is the common base class of Node Element (NE) and Leaf Element
 *    with the method      void E::setParent(std::shared_ptr<E> parent);
 *
 * I is the Item class
 */

template<typename NE, typename E, typename I>
class ElementTree
{
public:
    /**
     * @brief Return root element
     *
     * @return  weak_ptr to root element
     */
    std::weak_ptr<E> root() const { return rootElement; }

    /* Allow testing as 'if (tree) ...' */
    explicit operator bool() const { return !rootElement.expired(); }

    /**
     * @brief Find the existing part of an element path and return pointer to nearest node.
     *
     * @param[in] root  weak_ptr to the root of the element structure
     * @param[in,out] path  element path; existing leading nodes will be removed
     *
     * @return shared_ptr to the nearest existing node in the tree, shared_ptr<>() if no overlap
     */
    std::shared_ptr<E> nearestNode(std::list<std::string> &path)
    {
        bool found;

        if (path.empty())
            return std::shared_ptr<E>();

        // Starting from unnamed root node
        std::shared_ptr<E> elem = rootElement.lock();

        // Walk down the chain of children as long as names match
        do {
            auto part = path.begin();
            found = false;
            if (elem && !elem->isLeaf()) {
                auto nextelem = elem->findChild(*part);
                if (nextelem) {
                    found = true;
                    elem = std::move(nextelem);
                    path.pop_front();
                }
            }
        } while (found && !path.empty());

        return elem;
    }

    /**
     * @brief Add a new leaf element to the element tree.
     *
     * @param leaf  shared_ptr to the leaf to insert
     * @param path  full path of the leaf
     *
     * @throws runtime_error when trying to add elements to a leaf node
     *                       when adding with empty path but [ROOT] exists
     */
    void addLeaf (std::shared_ptr<E> leaf, const std::list<std::string> &fullpath, I *item)
    {
        std::shared_ptr<E> elem(leaf);
        std::list<std::string> path(fullpath);

        auto branch = nearestNode(path);

        if (branch && branch->isLeaf())
            throw std::runtime_error(SB() << "can't add leaf to existing leaf " << branch->name);
        if (path.empty()) {
            if (rootElement.lock())
                throw std::runtime_error(SB() << "root node does already exist");
            rootElement = elem;
        } else {
            path.pop_back(); // remove the leaf name
            for (auto rit = path.crbegin(); rit != path.crend(); ++rit) {
                auto node = std::make_shared<NE>(*rit, item);
                node->addChild(elem);
                elem->setParent(node);
                elem = std::move(node);
            }
            if (branch) {
                branch->addChild(elem);
                elem->setParent(branch);
            } else {
                auto node = std::make_shared<NE>("[ROOT]", item);
                node->addChild(elem);
                elem->setParent(node);
                rootElement = std::move(node);
            }
        }
    }

public:
    void setRoot(std::shared_ptr<E> root) { rootElement = root; }

private:
    std::weak_ptr<E> rootElement;
};

} // namespace DevOpcua

#endif // DEVOPCUA_ELEMENTTREE_H
