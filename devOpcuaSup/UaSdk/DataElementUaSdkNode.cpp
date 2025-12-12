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

#include "DataElementUaSdkNode.h"

#include <iostream>
#include <memory>
#include <string>

#include <opcua_builtintypes.h>
#include <statuscode.h>
#include <uaarraytemplates.h>
#include <uadatetime.h>
#include <uaenumdefinition.h>
#include <uaextensionobject.h>
#include <uagenericunionvalue.h>

#include <alarm.h>
#include <epicsTime.h>
#include <errlog.h>

#include "ItemUaSdk.h"
#include "RecordConnector.h"

namespace DevOpcua {

DataElementUaSdkNode::DataElementUaSdkNode (const std::string &name, class ItemUaSdk *item)
    : DataElementUaSdk(name, item)
    , timesrc(-1)
    , mapped(false)
{}

void
DataElementUaSdkNode::addChild (std::weak_ptr<DataElementUaSdk> elem)
{
    elements.push_back(elem);
}

std::shared_ptr<DataElementUaSdk>
DataElementUaSdkNode::findChild (const std::string &name) const
{
    for (const auto &it : elements)
        if (auto pit = it.lock())
            if (pit->name == name)
                return pit;
    return std::shared_ptr<DataElementUaSdk>();
}

void
DataElementUaSdkNode::show (const int level, const unsigned int indent) const
{
    std::string ind(indent * 2, ' ');
    std::cout << ind;
    std::cout << "node=" << name << " children=" << elements.size() << " mapped=" << (mapped ? "y" : "n") << "\n";
    for (const auto &it : elements) {
        if (auto pelem = it.lock()) {
            pelem->show(level, indent + 1);
        }
    }
}

void
DataElementUaSdkNode::createMap (const UaStructureDefinition &definition, const std::string *timefrom)
{
    if (debug() >= 5)
        std::cout << " ** creating index-to-element map for child elements" << std::endl;

    if (timefrom) {
        for (int i = 0; i < definition.childrenCount(); i++) {
            if (*timefrom == definition.child(i).name().toUtf8()) {
                timesrc = i;
            }
        }
        OpcUa_BuiltInType t = definition.child(timesrc).valueType();
        if (timesrc == -1) {
            errlogPrintf("%s: timestamp element %s not found - using source timestamp\n",
                         pitem->recConnector->getRecordName(),
                         timefrom->c_str());
        } else if (t != OpcUaType_DateTime) {
            errlogPrintf("%s: timestamp element %s has invalid type %s - using "
                         "source timestamp\n",
                         pitem->recConnector->getRecordName(),
                         timefrom->c_str(),
                         variantTypeString(t));
            timesrc = -1;
        }
    }
    for (const auto &it : elements) {
        auto pelem = it.lock();
        for (int i = 0; i < definition.childrenCount(); i++) {
            if (pelem->name == definition.child(i).name().toUtf8())
                elementMap.insert({i, it});
        }
    }
    if (debug() >= 5)
        std::cout << " ** " << elementMap.size() << "/" << elements.size() << " child elements mapped to a "
                  << "structure of " << definition.childrenCount() << " elements" << std::endl;
    mapped = true;
}

void
DataElementUaSdkNode::createMap (const UaLocalizedText &)
{
    for (const auto &it : elements) {
        auto pelem = it.lock();
        if (pelem->name == "locale")
            elementMap.insert({0, it});
        else if (pelem->name == "text")
            elementMap.insert({1, it});
        else
            errlogPrintf("Item %s %s element %s not found\n",
                         pitem->nodeid->toString().toUtf8(),
                         name.c_str(),
                         pelem->name.c_str());
    }
    mapped = true;
}

void
DataElementUaSdkNode::createMap (const UaQualifiedName &)
{
    for (const auto &it : elements) {
        auto pelem = it.lock();
        if (pelem->name == "namespaceIndex")
            elementMap.insert({0, it});
        else if (pelem->name == "name")
            elementMap.insert({1, it});
        else
            errlogPrintf("Item %s %s element %s not found\n",
                         pitem->nodeid->toString().toUtf8(),
                         name.c_str(),
                         pelem->name.c_str());
    }
    mapped = true;
}

void
DataElementUaSdkNode::setIncomingData (const UaVariant &value,
                                       ProcessReason reason,
                                       const std::string *timefrom,
                                       const UaNodeId *typeId)
{
    incomingData = value;

    if (debug() >= 5)
        std::cout << "Element " << name << " splitting structured data to " << elements.size() << " child elements"
                  << std::endl;

    switch (value.type()) {
    case OpcUaType_ExtensionObject: {
        UaExtensionObject extensionObject;
        value.toExtensionObject(extensionObject);
        if (extensionObject.encoding() == UaExtensionObject::EncodeableObject)
            extensionObject.changeEncoding(UaExtensionObject::Binary);

        UaStructureDefinition definition = pitem->structureDefinition(extensionObject.encodingTypeId());
        if (definition.isNull()) {
            errlogPrintf("Cannot get a structure definition for item %s element %s (dataTypeId %s "
                         "encodingTypeId %s) - check "
                         "access to type "
                         "dictionary\n",
                         pitem->nodeid->toString().toUtf8(),
                         name.c_str(),
                         extensionObject.dataTypeId().toString().toUtf8(),
                         extensionObject.encodingTypeId().toString().toUtf8());
            return;
        }

        if (!mapped)
            createMap(definition, timefrom);

        if (timefrom) {
            if (timesrc >= 0)
                pitem->tsData
                    = epicsTimeFromUaVariant(UaGenericStructureValue(extensionObject, definition).value(timesrc));
            else
                pitem->tsData = pitem->tsSource;
        }

        for (const auto &it : elementMap) {
            auto pelem = it.second.lock();
            OpcUa_StatusCode stat;
            if (definition.isUnion()) {
                UaGenericUnionValue genericValue(extensionObject, definition);
                int index = genericValue.switchValue() - 1;
                if (it.first == index) {
                    pelem->setIncomingData(genericValue.value(), reason);
                    stat = OpcUa_Good;
                } else {
                    stat = OpcUa_BadNoData;
                }
            } else {
                const UaVariant memberValue
                    = UaGenericStructureValue(extensionObject, definition).value(it.first, &stat);
                if (stat == OpcUa_Good) {
                    pelem->setIncomingData(memberValue, reason);
                }
            }
            if (stat == OpcUa_BadNoData) {
                const UaStructureField &field = definition.child(it.first);
                OpcUa_Variant fakeValue;
                OpcUa_Variant_Initialize(&fakeValue);
                fakeValue.Datatype = field.valueType();
                fakeValue.ArrayType = field.valueRank() != 0;
                if (debug())
                    std::cerr << pitem->recConnector->getRecordName() << " element " << pelem->name
                              << (definition.isUnion() ? " not taken choice " : " absent optional ")
                              << variantTypeString((OpcUa_BuiltInType) fakeValue.Datatype)
                              << (fakeValue.ArrayType ? " array" : " scalar") << std::endl;
                pelem->setIncomingData(fakeValue, ProcessReason::readFailure);
            }
        }
        break;
    }
    case OpcUaType_LocalizedText: {
        UaLocalizedText localizedText;
        value.toLocalizedText(localizedText);
        if (!mapped)
            createMap(localizedText);

        for (const auto &it : elementMap) {
            auto pelem = it.second.lock();
            UaVariant memberValue;
            switch (it.first) {
            case 0:
                memberValue.setString(localizedText.locale());
                break;
            case 1:
                memberValue.setString(localizedText.text());
                break;
            }
            pelem->setIncomingData(memberValue, reason);
        }
        break;
    }
    case OpcUaType_QualifiedName: {
        UaQualifiedName qualifiedName;
        value.toQualifiedName(qualifiedName);
        if (!mapped)
            createMap(qualifiedName);

        for (const auto &it : elementMap) {
            auto pelem = it.second.lock();
            UaVariant memberValue;
            switch (it.first) {
            case 0:
                memberValue.setUInt16(qualifiedName.namespaceIndex());
                break;
            case 1:
                memberValue.setString(qualifiedName.name());
                break;
            }
            pelem->setIncomingData(memberValue, reason);
        }
        break;
    }
    default:
        errlogPrintf("%s: %s is no structured data but a %s\n",
                     pitem->recConnector->getRecordName(),
                     name.c_str(),
                     variantTypeString(value.type()));
    }
}

void
DataElementUaSdkNode::setIncomingEvent (ProcessReason reason)
{
    for (const auto &it : elements) {
        auto pelem = it.lock();
        pelem->setIncomingEvent(reason);
    }
    if (reason == ProcessReason::connectionLoss) {
        elementMap.clear();
        timesrc = -1;
        mapped = false;
    }
}

void
DataElementUaSdkNode::setState (const ConnectionStatus state)
{
    for (const auto &it : elements) {
        auto pelem = it.lock();
        pelem->setState(state);
    }
}

const UaVariant &
DataElementUaSdkNode::getOutgoingData ()
{
    if (debug() >= 4)
        std::cout << "Element " << name << " updating structured data from " << elements.size() << " child elements"
                  << std::endl;

    outgoingData = incomingData;
    bool isdirty = false;
    switch (outgoingData.type()) {
    case OpcUaType_ExtensionObject: {
        UaExtensionObject extensionObject;
        outgoingData.toExtensionObject(extensionObject);
        if (extensionObject.encoding() == UaExtensionObject::EncodeableObject)
            extensionObject.changeEncoding(UaExtensionObject::Binary);

        UaStructureDefinition definition = pitem->structureDefinition(extensionObject.encodingTypeId());
        if (definition.isNull()) {
            errlogPrintf("Cannot get a structure definition for extensionObject with dataTypeID %s "
                         "/ encodingTypeID %s - "
                         "check access to type "
                         "dictionary\n",
                         extensionObject.dataTypeId().toString().toUtf8(),
                         extensionObject.encodingTypeId().toString().toUtf8());
            return outgoingData;
        }

        if (!mapped)
            createMap(definition);

        if (definition.isUnion()) {
            UaGenericUnionValue genericUnion(extensionObject, definition);
            for (const auto &it : elementMap) {
                auto pelem = it.second.lock();
                bool updated = false;
                {
                    Guard G(pelem->outgoingLock);
                    if (pelem->isDirty()) {
                        genericUnion.setValue(it.first + 1, pelem->getOutgoingData());
                        pelem->isdirty = false;
                        isdirty = true;
                        updated = true;
                    }
                }
                if (debug() >= 4)
                    std::cout << "Data from child element " << pelem->name
                              << (updated ? " inserted into union" : " ignored (not dirty)") << std::endl;
            }
            if (isdirty)
                genericUnion.toExtensionObject(extensionObject);

        } else {
            UaGenericStructureValue genericStruct(extensionObject, definition);
            for (const auto &it : elementMap) {
                auto pelem = it.second.lock();
                bool updated = false;
                {
                    Guard G(pelem->outgoingLock);
                    if (pelem->isDirty()) {
                        genericStruct.setField(it.first, pelem->getOutgoingData());
                        pelem->isdirty = false;
                        isdirty = true;
                        updated = true;
                    }
                }
                if (debug() >= 4)
                    std::cout << "Data from child element " << pelem->name
                              << (updated ? " inserted into structure" : " ignored (not dirty)") << std::endl;
            }
            if (isdirty)
                genericStruct.toExtensionObject(extensionObject);
        }

        if (isdirty)
            outgoingData.setExtensionObject(extensionObject, OpcUa_True);
        break;
    }
    case OpcUaType_LocalizedText: {
        UaLocalizedText localizedText;
        outgoingData.toLocalizedText(localizedText);
        if (!mapped)
            createMap(localizedText);

        for (const auto &it : elementMap) {
            auto pelem = it.second.lock();
            bool updated = false;
            {
                Guard G(pelem->outgoingLock);
                if (pelem->isDirty()) {
                    switch (it.first) {
                    case 0:
                        localizedText.setLocale(pelem->getOutgoingData().toString());
                        break;
                    case 1:
                        localizedText.setText(pelem->getOutgoingData().toString());
                        break;
                    }
                    pelem->isdirty = false;
                    isdirty = true;
                    updated = true;
                }
                if (debug() >= 4)
                    std::cout << "Data from child element " << pelem->name
                              << (updated ? " inserted into LocalizedText" : " ignored (not dirty)") << std::endl;
            }
        }
        if (isdirty)
            outgoingData.setLocalizedText(localizedText);
        break;
    }
    case OpcUaType_QualifiedName: {
        UaQualifiedName qualifiedName;
        outgoingData.toQualifiedName(qualifiedName);
        if (!mapped)
            createMap(qualifiedName);

        for (const auto &it : elementMap) {
            auto pelem = it.second.lock();
            bool updated = false;
            {
                Guard G(pelem->outgoingLock);
                OpcUa_UInt16 ns;
                if (pelem->isDirty()) {
                    switch (it.first) {
                    case 0:
                        if (pelem->getOutgoingData().toUInt16(ns) == OpcUa_Good)
                            qualifiedName.setNamespaceIndex(ns);
                        break;
                    case 1:
                        qualifiedName.setQualifiedName(pelem->getOutgoingData().toString(),
                                                       qualifiedName.namespaceIndex());
                        break;
                    }
                    pelem->isdirty = false;
                    isdirty = true;
                    updated = true;
                }
                if (debug() >= 4)
                    std::cout << "Data from child element " << pelem->name
                              << (updated ? " inserted into QualifiedName" : " ignored (not dirty)") << std::endl;
            }
        }
        if (isdirty)
            outgoingData.setQualifiedName(qualifiedName);
        break;
    }
    default:
        errlogPrintf("%s: %s is no structured data but a %s\n",
                     pitem->recConnector->getRecordName(),
                     name.c_str(),
                     variantTypeString(outgoingData.type()));
        return outgoingData;
    }
    if (debug() >= 4) {
        if (isdirty)
            std::cout << "Encoding changed data structure to outgoingData of element " << name << std::endl;
        else
            std::cout << "Returning unchanged outgoingData of element " << name << std::endl;
    }
    return outgoingData;
}

void
DataElementUaSdkNode::clearOutgoingData ()
{
    outgoingData.clear();
}

void
DataElementUaSdkNode::requestRecordProcessing (const ProcessReason reason) const
{
    for (const auto &it : elementMap) {
        auto pelem = it.second.lock();
        pelem->requestRecordProcessing(reason);
    }
}

epicsTime
DataElementUaSdkNode::epicsTimeFromUaVariant (const UaVariant &data) const
{
    UaDateTime dt;
    UaStatus s = data.toDateTime(dt);
    if (s.isGood()) {
        return ItemUaSdk::uaToEpicsTime(dt, 0);
    }
    return pitem->tsSource;
}

#define LOG_ERROR_AND_RETURN(method) \
    errlogPrintf("DataElementUaSdkNode::%s called on a node element. This should not happen.\n", #method); \
    return 1;
} // namespace DevOpcua
