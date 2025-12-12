/*************************************************************************\
* Copyright (c) 2018-2025 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 *  and example code from the Unified Automation C++ Based OPC UA Client SDK
 */

#ifndef DEVOPCUA_DATAELEMENTUASDKNODE_H
#define DEVOPCUA_DATAELEMENTUASDKNODE_H

#include <vector>
#include <unordered_map>
#include <memory>

#include <uavariant.h>

#include "DataElementUaSdk.h"

namespace DevOpcua {

class DataElementUaSdkNode : public DataElementUaSdk
{
public:
    DataElementUaSdkNode(const std::string &name, class ItemUaSdk *item);
    virtual ~DataElementUaSdkNode () override {}

    /* ElementTree node interface methods */
    virtual bool isLeaf() const override { return false; }
    virtual void addChild(std::weak_ptr<DataElementUaSdk> elem) override;
    virtual std::shared_ptr<DataElementUaSdk> findChild(const std::string &name) const override;

    virtual void show(const int level, const unsigned int indent) const override;

    virtual void setIncomingData(const UaVariant &value,
                                 ProcessReason reason,
                                 const std::string *timefrom = nullptr,
                                 const UaNodeId *typeId = nullptr) override;
    virtual void setIncomingEvent(ProcessReason reason) override;
    virtual void setState(const ConnectionStatus state) override;
    virtual const UaVariant &getOutgoingData() override;
    virtual void clearOutgoingData() override;
    virtual void requestRecordProcessing(const ProcessReason reason) const override;

private:
    virtual bool isDirty() const override { return true; }
    virtual void markAsDirty() override {}

    void createMap(const UaStructureDefinition& definition, const std::string *timefrom = nullptr);
    void createMap(const UaLocalizedText&);
    void createMap(const UaQualifiedName&);

    epicsTime epicsTimeFromUaVariant(const UaVariant &data) const;

    std::vector<std::weak_ptr<DataElementUaSdk>> elements;
    std::unordered_map<int, std::weak_ptr<DataElementUaSdk>> elementMap;
    int timesrc;
    bool mapped;
};

}

#endif // DEVOPCUA_DATAELEMENTUASDKNODE_H
