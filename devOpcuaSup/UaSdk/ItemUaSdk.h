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

#include <statuscode.h>
#include <opcua_builtintypes.h>

#include "Item.h"
#include "SessionUaSdk.h"

namespace DevOpcua {

using namespace UaClientSdk;

class DataElementUaSdk;
class SubscriptionUaSdk;
struct linkInfo;

/**
 * @brief The ItemUaSdk inplementation of an OPC UA item.
 *
 * See DevOpcua::Item
 */
class ItemUaSdk : public Item
{
public:
    /**
     * @brief Constructor for an OPC UA item (implementation).
     * @param info  configuration as parsed from the EPICS database
     */
    ItemUaSdk(const linkInfo &info);
    ~ItemUaSdk() override;

    /**
     * @brief Request beginRead service. See DevOpcua::Item::requestRead
     */
    virtual void requestRead() override { session->requestRead(*this); }

    /**
     * @brief Request beginWrite service. See DevOpcua::Item::requestWrite
     */
    virtual void requestWrite() override { session->requestWrite(*this); }

    /**
     * @brief Print configuration and status. See DevOpcua::Item::show
     */
    virtual void show(int level) const override;

    /**
     * @brief Return monitored status. See DevOpcua::Item::isMonitored
     */
    virtual bool isMonitored() const override { return !!subscription; }

    /**
     * @brief Getter that returns the node id of this item.
     * @return node id
     */
    UaNodeId &getNodeId() const { return (*nodeid); }

    /**
     * @brief Setter for the status of a read operation.
     * @param status  status code received by the client library
     */
    void setReadStatus(const OpcUa_StatusCode &status) { readStatus = status; }

    /**
     * @brief Getter for the status of the last read operation.
     * @return read status
     */
    const UaStatusCode &getReadStatus() { return readStatus; }

    /**
     * @brief Setter for the status of a write operation.
     * @param status  status code received by the client library
     */
    void setWriteStatus(const OpcUa_StatusCode &status) { writeStatus = status; }

    /**
     * @brief Getter for the status of the last write operation.
     * @return write status
     */
    const UaStatusCode &getWriteStatus() { return writeStatus; }

    /**
     * @brief Getter to access the top data element of this item
     * @return top data element
     */
    DataElementUaSdk &data() { return (*dataElement); }

private:
    SubscriptionUaSdk *subscription;   /**< pointer to subscription (if monitored) */
    SessionUaSdk *session;             /**< pointer to session */
    std::unique_ptr<UaNodeId> nodeid;  /**< node id of this item */
    std::unique_ptr<DataElementUaSdk> dataElement;  /**< top level data element */
    UaStatusCode readStatus;           /**< status code of last read service */
    UaStatusCode writeStatus;          /**< status code of last write service */
};

} // namespace DevOpcua

#endif // DEVOPCUA_ITEMUASDK_H
