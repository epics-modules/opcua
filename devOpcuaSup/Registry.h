/*************************************************************************\
* Copyright (c) 2020-2021 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#ifndef DEVOPCUA_REGISTRY_H
#define DEVOPCUA_REGISTRY_H

#include <set>
#include <map>
#include <string>

#include <epicsMutex.h>
#include <epicsGuard.h>

namespace DevOpcua {

/**
 * @brief Helper meta registry to keep names unique.
 *
 * Union set of keys from multiple registries that helps to keep
 * names (keys) unique across multiple registries.
 */

class RegistryKeyNamespace
{
public:
    RegistryKeyNamespace() {}

    void
    insert(const std::string name)
    {
        names.insert(name);
    }

    bool
    contains(const std::string &name)
    {
        return (names.find(name) != names.end());
    }

    epicsMutex lock;
    static RegistryKeyNamespace global; ///< default global registry namespace

private:
    std::set<std::string> names;
};

/**
 * @brief A registry for managing named sessions and subscriptions.
 *
 * Names will be kept unique across the whole namespace (session and
 * subscription names).
 *
 * The template parameter T is the implementation specific object class
 * (i.e., the class of the things to be managed).
 * Objects are added by calling insert() with a pair of name (key) and
 * a pointer to the object.
 */

template<typename T>
class Registry
{
public:
    Registry(RegistryKeyNamespace &keys = RegistryKeyNamespace::global)
        : keys(keys)
    {}

    /**
     * @brief Insert an object into the registry.
     *
     * @param object  pair of { name, object* } to insert
     * @return  0 if successful, -1 on error
     *
     */
    long
    insert(std::pair<const std::string, T *> object)
    {
        long status = -1;

        epicsGuard<epicsMutex> G(keys.lock);
        if (!keys.contains(object.first)) {
            registry.insert(object);
            keys.insert(object.first);
            status = 0;
        }
        return status;
    }

    /**
     * @brief Find an object by name.
     *
     * @param name  object name to search for
     *
     * @return  pointer to object, nullptr if not found
     */
    T *
    find(const std::string &name)
    {
        auto it = registry.find(name);
        if (it == registry.end())
            return nullptr;
        else
            return it->second;
    }

    /**
     * @brief Check for the presence of a name in the registry.
     *
     * @param name  object name to check
     * @return  true if exists
     */
    bool
    contains(const std::string &name)
    {
        return !!find(name);
    }

    /**
     * @brief Return the size of the registry, i.e the number of registered elements.
     *
     * @return  size of the registry
     */
    size_t
    size() const noexcept
    {
        return registry.size();
    }

private:
    RegistryKeyNamespace &keys;
    std::map<std::string, T *> registry;
};

} // namespace DevOpcua

#endif // DEVOPCUA_REGISTRY_H
