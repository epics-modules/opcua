/* WARNING: This is a generated file.
 * Any manual changes will be overwritten. */

#include "opcuaTestNodeSet.h"


/* Simulation - ns=1;s=Sim */

static UA_StatusCode function_opcuaTestNodeSet_0_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_ObjectAttributes attr = UA_ObjectAttributes_default;
attr.displayName = UA_LOCALIZEDTEXT("", "Simulation");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_OBJECT,
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 85LU),
UA_NODEID_NUMERIC(ns[0], 35LU),
UA_QUALIFIEDNAME(ns[1], "Simulation"),
UA_NODEID_NUMERIC(ns[0], 61LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_OBJECTATTRIBUTES],NULL, NULL);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_0_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim")
);
}

/* TestVarInt64 - ns=1;s=Sim.TestVarInt64 */

static UA_StatusCode function_opcuaTestNodeSet_1_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 8LU);
UA_Int64 *variablenode_ns_1_s_sim_testvarint64_variant_DataContents =  UA_Int64_new();
if (!variablenode_ns_1_s_sim_testvarint64_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_Int64_init(variablenode_ns_1_s_sim_testvarint64_variant_DataContents);
*variablenode_ns_1_s_sim_testvarint64_variant_DataContents = (UA_Int64) -1294967296;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvarint64_variant_DataContents, &UA_TYPES[UA_TYPES_INT64]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarInt64");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarInt64"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarInt64"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_Int64_delete(variablenode_ns_1_s_sim_testvarint64_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_1_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarInt64")
);
}

/* TestVarInt16 - ns=1;s=Sim.TestVarInt16 */

static UA_StatusCode function_opcuaTestNodeSet_2_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 4LU);
UA_Int16 *variablenode_ns_1_s_sim_testvarint16_variant_DataContents =  UA_Int16_new();
if (!variablenode_ns_1_s_sim_testvarint16_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_Int16_init(variablenode_ns_1_s_sim_testvarint16_variant_DataContents);
*variablenode_ns_1_s_sim_testvarint16_variant_DataContents = (UA_Int16) -32768;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvarint16_variant_DataContents, &UA_TYPES[UA_TYPES_INT16]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarInt16");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarInt16"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarInt16"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_Int16_delete(variablenode_ns_1_s_sim_testvarint16_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_2_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarInt16")
);
}

/* TestVarUInt16 - ns=1;s=Sim.TestVarUInt16 */

static UA_StatusCode function_opcuaTestNodeSet_3_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 5LU);
UA_UInt16 *variablenode_ns_1_s_sim_testvariuint16_variant_DataContents =  UA_UInt16_new();
if (!variablenode_ns_1_s_sim_testvariuint16_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_UInt16_init(variablenode_ns_1_s_sim_testvariuint16_variant_DataContents);
*variablenode_ns_1_s_sim_testvariuint16_variant_DataContents = (UA_UInt16) 65535;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvariuint16_variant_DataContents, &UA_TYPES[UA_TYPES_UINT16]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarUInt16");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarUInt16"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarUInt16"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_UInt16_delete(variablenode_ns_1_s_sim_testvariuint16_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_3_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarUInt16")
);
}

/* TestVarString - ns=1;s=Sim.TestVarString */

static UA_StatusCode function_opcuaTestNodeSet_4_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 12LU);
UA_String *variablenode_ns_1_s_sim_testvarstring_variant_DataContents =  UA_String_new();
if (!variablenode_ns_1_s_sim_testvarstring_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_String_init(variablenode_ns_1_s_sim_testvarstring_variant_DataContents);
*variablenode_ns_1_s_sim_testvarstring_variant_DataContents = UA_STRING_ALLOC("TestString01");
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvarstring_variant_DataContents, &UA_TYPES[UA_TYPES_STRING]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarString");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarString"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarString"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_String_delete(variablenode_ns_1_s_sim_testvarstring_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_4_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarString")
);
}

/* TestVarByte - ns=1;s=Sim.TestVarByte */

static UA_StatusCode function_opcuaTestNodeSet_5_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 3LU);
UA_Byte *variablenode_ns_1_s_sim_testvarbyte_variant_DataContents =  UA_Byte_new();
if (!variablenode_ns_1_s_sim_testvarbyte_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_Byte_init(variablenode_ns_1_s_sim_testvarbyte_variant_DataContents);
*variablenode_ns_1_s_sim_testvarbyte_variant_DataContents = (UA_Byte) 255;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvarbyte_variant_DataContents, &UA_TYPES[UA_TYPES_BYTE]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarByte");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarByte"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarByte"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_Byte_delete(variablenode_ns_1_s_sim_testvarbyte_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_5_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarByte")
);
}

/* TestVarDouble - ns=1;s=Sim.TestVarDouble */

static UA_StatusCode function_opcuaTestNodeSet_6_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 11LU);
UA_Double *variablenode_ns_1_s_sim_testvardouble_variant_DataContents =  UA_Double_new();
if (!variablenode_ns_1_s_sim_testvardouble_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_Double_init(variablenode_ns_1_s_sim_testvardouble_variant_DataContents);
*variablenode_ns_1_s_sim_testvardouble_variant_DataContents = (UA_Double) 0.002;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvardouble_variant_DataContents, &UA_TYPES[UA_TYPES_DOUBLE]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarDouble");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarDouble"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarDouble"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_Double_delete(variablenode_ns_1_s_sim_testvardouble_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_6_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarDouble")
);
}

/* TestVarUInt64 - ns=1;s=Sim.TestVarUInt64 */

static UA_StatusCode function_opcuaTestNodeSet_7_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 9LU);
UA_UInt64 *variablenode_ns_1_s_sim_testvaruint64_variant_DataContents =  UA_UInt64_new();
if (!variablenode_ns_1_s_sim_testvaruint64_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_UInt64_init(variablenode_ns_1_s_sim_testvaruint64_variant_DataContents);
*variablenode_ns_1_s_sim_testvaruint64_variant_DataContents = (UA_UInt64) 18446744073709551615;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvaruint64_variant_DataContents, &UA_TYPES[UA_TYPES_UINT64]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarUInt64");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarUInt64"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarUInt64"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_UInt64_delete(variablenode_ns_1_s_sim_testvaruint64_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_7_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarUInt64")
);
}

/* TestRamp - ns=1;s=Sim.TestRamp */

static UA_StatusCode function_opcuaTestNodeSet_8_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 11LU);
UA_Double *variablenode_ns_1_s_sim_testramp_variant_DataContents =  UA_Double_new();
if (!variablenode_ns_1_s_sim_testramp_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_Double_init(variablenode_ns_1_s_sim_testramp_variant_DataContents);
*variablenode_ns_1_s_sim_testramp_variant_DataContents = (UA_Double) 0;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testramp_variant_DataContents, &UA_TYPES[UA_TYPES_DOUBLE]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestRamp");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestRamp"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestRamp"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_Double_delete(variablenode_ns_1_s_sim_testramp_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_8_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestRamp")
);
}

/* TestVarBool - ns=1;s=Sim.TestVarBool */

static UA_StatusCode function_opcuaTestNodeSet_9_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 1LU);
UA_Boolean *variablenode_ns_1_s_sim_testvarbool_variant_DataContents =  UA_Boolean_new();
if (!variablenode_ns_1_s_sim_testvarbool_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_Boolean_init(variablenode_ns_1_s_sim_testvarbool_variant_DataContents);
*variablenode_ns_1_s_sim_testvarbool_variant_DataContents = (UA_Boolean) true;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvarbool_variant_DataContents, &UA_TYPES[UA_TYPES_BOOLEAN]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarBool");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarBool"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarBool"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_Boolean_delete(variablenode_ns_1_s_sim_testvarbool_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_9_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarBool")
);
}

/* TestVarFloat - ns=1;s=Sim.TestVarFloat */

static UA_StatusCode function_opcuaTestNodeSet_10_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 10LU);
UA_Float *variablenode_ns_1_s_sim_testvarfloat_variant_DataContents =  UA_Float_new();
if (!variablenode_ns_1_s_sim_testvarfloat_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_Float_init(variablenode_ns_1_s_sim_testvarfloat_variant_DataContents);
*variablenode_ns_1_s_sim_testvarfloat_variant_DataContents = (UA_Float) 0.001;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvarfloat_variant_DataContents, &UA_TYPES[UA_TYPES_FLOAT]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarFloat");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarFloat"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarFloat"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_Float_delete(variablenode_ns_1_s_sim_testvarfloat_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_10_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarFloat")
);
}

/* TestVarUInt32 - ns=1;s=Sim.TestVarUInt32 */

static UA_StatusCode function_opcuaTestNodeSet_11_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 7LU);
UA_UInt32 *variablenode_ns_1_s_sim_testvaruint32_variant_DataContents =  UA_UInt32_new();
if (!variablenode_ns_1_s_sim_testvaruint32_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_UInt32_init(variablenode_ns_1_s_sim_testvaruint32_variant_DataContents);
*variablenode_ns_1_s_sim_testvaruint32_variant_DataContents = (UA_UInt32) 4294967295;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvaruint32_variant_DataContents, &UA_TYPES[UA_TYPES_UINT32]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarUInt32");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarUInt32"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarUInt32"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_UInt32_delete(variablenode_ns_1_s_sim_testvaruint32_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_11_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarUInt32")
);
}

/* TestVarSByte - ns=1;s=Sim.TestVarSByte */

static UA_StatusCode function_opcuaTestNodeSet_12_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 2LU);
UA_SByte *variablenode_ns_1_s_sim_testvarsbyte_variant_DataContents =  UA_SByte_new();
if (!variablenode_ns_1_s_sim_testvarsbyte_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_SByte_init(variablenode_ns_1_s_sim_testvarsbyte_variant_DataContents);
*variablenode_ns_1_s_sim_testvarsbyte_variant_DataContents = (UA_SByte) -128;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvarsbyte_variant_DataContents, &UA_TYPES[UA_TYPES_SBYTE]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarSByte");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarSByte"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarSByte"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_SByte_delete(variablenode_ns_1_s_sim_testvarsbyte_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_12_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarSByte")
);
}

/* TestVarInt32 - ns=1;s=Sim.TestVarInt32 */

static UA_StatusCode function_opcuaTestNodeSet_13_begin(UA_Server *server, UA_UInt16* ns) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
UA_VariableAttributes attr = UA_VariableAttributes_default;
attr.minimumSamplingInterval = 0.000000;
attr.userAccessLevel = 1;
attr.accessLevel = 3;
/* Value rank inherited */
attr.valueRank = -1;
attr.dataType = UA_NODEID_NUMERIC(ns[0], 6LU);
UA_Int32 *variablenode_ns_1_s_sim_testvarint32_variant_DataContents =  UA_Int32_new();
if (!variablenode_ns_1_s_sim_testvarint32_variant_DataContents) return UA_STATUSCODE_BADOUTOFMEMORY;
UA_Int32_init(variablenode_ns_1_s_sim_testvarint32_variant_DataContents);
*variablenode_ns_1_s_sim_testvarint32_variant_DataContents = (UA_Int32) -2147483648;
UA_Variant_setScalar(&attr.value, variablenode_ns_1_s_sim_testvarint32_variant_DataContents, &UA_TYPES[UA_TYPES_INT32]);
attr.displayName = UA_LOCALIZEDTEXT("", "TestVarInt32");
retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_VARIABLE,
UA_NODEID_STRING(ns[1], "Sim.TestVarInt32"),
UA_NODEID_STRING(ns[1], "Sim"),
UA_NODEID_NUMERIC(ns[0], 47LU),
UA_QUALIFIEDNAME(ns[1], "TestVarInt32"),
UA_NODEID_NUMERIC(ns[0], 63LU),
(const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],NULL, NULL);
UA_Int32_delete(variablenode_ns_1_s_sim_testvarint32_variant_DataContents);
return retVal;
}

static UA_StatusCode function_opcuaTestNodeSet_13_finish(UA_Server *server, UA_UInt16* ns) {
return UA_Server_addNode_finish(server, 
UA_NODEID_STRING(ns[1], "Sim.TestVarInt32")
);
}

UA_StatusCode opcuaTestNodeSet(UA_Server *server) {
UA_StatusCode retVal = UA_STATUSCODE_GOOD;
/* Use namespace ids generated by the server */
UA_UInt16 ns[2];
ns[0] = UA_Server_addNamespace(server, "http://opcfoundation.org/UA/");
ns[1] = UA_Server_addNamespace(server, "http://ess.eu/OpcUa");

/* Load custom datatype definitions into the server */
bool dummy = (
!(retVal = function_opcuaTestNodeSet_0_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_1_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_2_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_3_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_4_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_5_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_6_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_7_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_8_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_9_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_10_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_11_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_12_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_13_begin(server, ns))
&& !(retVal = function_opcuaTestNodeSet_13_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_12_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_11_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_10_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_9_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_8_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_7_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_6_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_5_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_4_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_3_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_2_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_1_finish(server, ns))
&& !(retVal = function_opcuaTestNodeSet_0_finish(server, ns))
); (void)(dummy);
return retVal;
}
