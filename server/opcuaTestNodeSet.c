/* WARNING: This is a generated file.
 * Any manual changes will be overwritten. */

#include "opcuaTestNodeSet.h"


/* Simulation - ns=1;s=85/0:Simulation */

static UA_StatusCode function_cfhvmsNodeSet_0_begin(UA_Server *server, UA_UInt16* ns) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("", "Simulation");
    #ifdef UA_ENABLE_NODESET_COMPILER_DESCRIPTIONS
    attr.description = UA_LOCALIZEDTEXT("", "The type for objects that organize other nodes.");
    #endif
    retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_OBJECT,
        UA_NODEID_STRING(ns[1], "85/0:Simulation"),
        UA_NODEID_NUMERIC(ns[0], 85),
        UA_NODEID_NUMERIC(ns[0], 35),
        UA_QUALIFIEDNAME(ns[1], "Simulation"),
        UA_NODEID_NUMERIC(ns[0], 61),
        (const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_OBJECTATTRIBUTES],NULL, NULL);
    return retVal;
    }
    
static UA_StatusCode function_cfhvmsNodeSet_0_finish(UA_Server *server, UA_UInt16* ns) {
    return UA_Server_addNode_finish(server, UA_NODEID_STRING(ns[1], "85/0:Simulation"));
}

/* 0:\\Test-01 - ns=1;s=0\\Test-01 */

static UA_StatusCode function_cfhvmsNodeSet_1_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = -1.000000;
attr.userAccessLevel = 3;
attr.accessLevel = 3;
attr.valueRank = -2;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 11);
UA_Double *variablenode_ns_1_s_0_test_01_variant_DataContents =  UA_Double_new();
if (!variablenode_ns_1_s_0_test_01_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_Double_init(variablenode_ns_1_s_0_test_01_variant_DataContents);
*variablenode_ns_1_s_0_test_01_variant_DataContents = (UA_Double) 3.117;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_0_test_01_variant_DataContents, &UA_TYPES[UA_TYPES_DOUBLE]);
attr.displayName = UA_LOCALIZEDTEXT("", "0:\\\\Test-01");
#ifdef UA_ENABLE_NODESET_COMPILER_DESCRIPTIONS
attr.description = UA_LOCALIZEDTEXT("", "The type for variable that represents a process value.");
#endif
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "0:\\\\Test-01"),
UA_NODEID_STRING(ns[1], "85/0:Simulation"),
UA_NODEID_NUMERIC(ns[0], 47),
UA_QUALIFIEDNAME(ns[1], "0:\\\\Test-01"),
UA_NODEID_NUMERIC(ns[0], 63),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_Double_delete(variablenode_ns_1_s_0_test_01_variant_DataContents);
return retVal;
}

static UA_StatusCode function_cfhvmsNodeSet_1_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "0:\\\\Test-01")
);
}


UA_StatusCode cfhvmsNodeSet(UA_Server *server, UA_UInt16 *ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
bool dummy = (
!(retVal = function_cfhvmsNodeSet_0_begin(server, ns))
&& !(retVal = function_cfhvmsNodeSet_1_begin(server, ns))
&& !(retVal = function_cfhvmsNodeSet_1_finish(server, ns))
&& !(retVal = function_cfhvmsNodeSet_0_finish(server, ns))
); (void)(dummy);
return retVal;
}
