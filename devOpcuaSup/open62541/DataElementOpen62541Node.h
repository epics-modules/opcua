/*************************************************************************\
* Copyright (c) 2018-2023 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

#ifndef DEVOPCUA_DATAELEMENTOPEN62541NODE_H
#define DEVOPCUA_DATAELEMENTOPEN62541NODE_H

#include "DataElementOpen62541.h"

namespace DevOpcua {

class DataElementOpen62541Node : public DataElementOpen62541
{
public:
    DataElementOpen62541Node(const std::string &name,
                             ItemOpen62541 *item);
    virtual ~DataElementOpen62541Node() override;

    /* ElementTree node interface methods */
    virtual bool isLeaf() const override { return false; }
    virtual void addChild(std::weak_ptr<DataElementOpen62541> elem) override
    {
        elements.push_back(elem);
    }
    virtual std::shared_ptr<DataElementOpen62541> findChild(const std::string &name) const override
    {
        for (const auto &it : elements)
            if (auto pit = it.lock())
                if (pit->name == name)
                    return pit;
        return std::shared_ptr<DataElementOpen62541>();
    }

    virtual void show(const int level, const unsigned int indent) const override;

    virtual void setIncomingData(const UA_Variant &value,
                         ProcessReason reason,
                         const std::string *timefrom = nullptr) override;
    virtual void setIncomingEvent(ProcessReason reason) override;
    virtual void setState(const ConnectionStatus state) override;
    virtual const UA_Variant &getOutgoingData() override;
    virtual void clearOutgoingData() override;
    virtual void requestRecordProcessing(const ProcessReason reason) const override;

private:
    virtual bool isDirty() const override { return true; }
    virtual void markAsDirty() override {}

    bool updateDataInStruct(void* container, std::shared_ptr<DataElementOpen62541> pelem);
    void createMap(const UA_DataType *type, const std::string* timefrom = nullptr);

    std::vector<std::weak_ptr<DataElementOpen62541>> elements;  /**< children (if node) */
    std::unordered_map<int, std::weak_ptr<DataElementOpen62541>> elementMap;
    ptrdiff_t timesrc;
    bool mapped;                             /**< child name to index mapping done */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTOPEN62541NODE_H
