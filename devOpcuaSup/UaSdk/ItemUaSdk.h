/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 *  and example code from the Unified Automation C++ Based OPC UA Client SDK
 */

#ifndef DEVOPCUA_ITEMUASDK_H
#define DEVOPCUA_ITEMUASDK_H

#include <memory>

#include <uaclientsdk.h>

#include "Item.h"
#include "SessionUaSdk.h"
#include "DataElementUaSdk.h"

namespace DevOpcua {

using namespace UaClientSdk;

class SubscriptionUaSdk;
struct linkInfo;

/**
 * @brief The ItemUaSdk inplementation of an OPC UA item.
 *
 * See DevOpcua::Item
 */
class ItemUaSdk : public Item, public DataElementUaSdk
{
public:
    /**
     * @brief Constructor for an OPC UA item (implementation).
     * @param info  configuration as parsed from the EPICS database
     */
    ItemUaSdk(const linkInfo &info);
    ~ItemUaSdk();

    /**
     * @brief Request beginRead service. See DevOpcua::Item::requestRead
     */
    void requestRead() override { session->requestRead((*this)); }

    /**
     * @brief Request beginWrite service. See DevOpcua::Item::requestWrite
     */
    void requestWrite() override {}

    /**
     * @brief Print configuration and status. See DevOpcua::Item::show
     */
    void show(int level) const override;

    /**
     * @brief Return monitored status. See DevOpcua::Item::isMonitored
     */
    bool isMonitored() const override { return !!subscription; }

    /**
     * @brief Getter that returns the node id of this item.
     * @return node id
     */
    UaNodeId &getNodeId() const { return (*nodeid); }

private:
    SubscriptionUaSdk *subscription;
    SessionUaSdk *session;
    std::unique_ptr<UaNodeId> nodeid;
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEMUASDK_H
