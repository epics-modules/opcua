/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 */

#ifndef DEVOPCUA_DATAELEMENT_H
#define DEVOPCUA_DATAELEMENT_H

#include <map>

#include <epicsTypes.h>
#include <epicsTime.h>

#include "devOpcua.h"

namespace DevOpcua {

typedef std::map<epicsUInt32, std::string> EnumChoices;

class RecordConnector;

/**
 * @brief The DataElement interface for a single piece of data.
 *
 * A data element can either be the top level data of an item (in that case
 * its name is an empty string) or be an element of a structured data type (in
 * that case name is the data element name).
 *
 * Inside a structure, a data element can either be a leaf, i.e. be of one of the
 * builtin types and connected to a record (through the pconnector member) or be
 * a node of a structured data type and contain a list of its child elements.
 *
 * As resource conflicts can only occur in nodes that are accessed by records
 * (database side) and items (OPC UA side), the RecordConnector lock must be held
 * when operating on a data element.
 */
class DataElement
{
public:
    virtual ~DataElement() {}

    /**
     * @brief "Factory" method to construct a linked list of data elements between a record connector and an item.
     *
     * Creates the leaf element first, then identifies the part of the path that already exists
     * on the item and creates the missing list of linked nodes.
     *
     * @param pitem       pointer to corresponding Item
     * @param pconnector  pointer to record connector to link to
     * @param elementPath  path of leaf element inside the structure
     */
    static void addElementToTree(Item *item,
                                 RecordConnector *pconnector,
                                 const std::list<std::string> &elementPath);

    /**
     * @brief Setter to create a (bidirectional) link to a RecordConnector.
     *
     * Sets the internal pointer to the record connector as well as the
     * inverse link in the record connector that points back.
     *
     * An existing link is cleanly removed before the new link is set up.
     *
     * @param connector  pointer to the RecordConnector to link to
     */
    void setRecordConnector(RecordConnector *connector);

    /**
     * @brief Print configuration and status on stdout.
     *
     * The verbosity level controls the amount of information:
     * 0 = one line
     *
     * @param level   verbosity level
     * @param indent  indentation level
     */
    virtual void show(const int level, const unsigned int indent) const = 0;

    /**
     * @brief Read incoming data as a scalar epicsInt32.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target EPICS type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and returns the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  set to value
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readScalar(epicsInt32 *value,
                            dbCommon *prec,
                            ProcessReason *nextReason = nullptr,

                            epicsUInt32 *statusCode = nullptr,
                            char *statusText = nullptr,
                            const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as a scalar epicsInt64.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target EPICS type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and returns the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  set to value
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readScalar(epicsInt64 *value,
                            dbCommon *prec,
                            ProcessReason *nextReason = nullptr,
                            epicsUInt32 *statusCode = nullptr,
                            char *statusText = nullptr,
                            const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as a scalar epicsUInt32.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target EPICS type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and returns the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  set to value
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readScalar(epicsUInt32 *value,
                            dbCommon *prec,
                            ProcessReason *nextReason = nullptr,
                            epicsUInt32 *statusCode = nullptr,
                            char *statusText = nullptr,
                            const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as a scalar epicsFloat64.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target EPICS type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and returns the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  set to value
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readScalar(epicsFloat64 *value,
                            dbCommon *prec,
                            ProcessReason *nextReason = nullptr,
                            epicsUInt32 *statusCode = nullptr,
                            char *statusText = nullptr,
                            const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as a classic C string (null terminated array of char).
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and returns the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  target string buffer
     * @param[in] len  length of target string buffer (including null byte)
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] lenRead  actual number of chars read (may be NULL)
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readScalar(char *value, const epicsUInt32 len,
                            dbCommon *prec,
                            ProcessReason *nextReason = nullptr,
                            epicsUInt32* lenRead = nullptr,
                            epicsUInt32 *statusCode = nullptr,
                            char *statusText = nullptr,
                            const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsInt8.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsInt8 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsUInt8.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsUInt8 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsInt16.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsInt16 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsUInt16.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsUInt16 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsInt32.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsInt32 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsUInt32.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsUInt32 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsInt64.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsInt64 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsUInt64.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsUInt64 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsFloat32.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsFloat32 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of epicsFloat64.
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(epicsFloat64 *value, const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Read incoming data as array of EPICS String type (char[MAX_STRING_SIZE]).
     *
     * Takes the next element off the queue of incoming data, converts it to the
     * target type, sets the record's STAT/SEVR according to the ProcessReason,
     * status code and success of the conversion, and writes the value, the ProcessReason
     * of the next queue element, the time stamp, the status code and text of
     * the OPC UA status of the item related to this DataElement.
     *
     * @param[out] value  pointer to target array
     * @param[in] len  length of an EPICS String element
     * @param[in] num  target array size
     * @param[out] numRead  number of elements written
     * @param[in] prec  pointer to EPICS record
     * @param[out] nextReason  set to ProcessReason for the next update, `none` if last element
     * @param[out] statusCode  set to the OPC UA status code of the update
     * @param[out] statusText  set to the OPC UA status text of the update (will be null terminated)
     * @param[in] statusTextLen  length of the statusText buffer
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long readArray(char *value, const epicsUInt32 len,
                           const epicsUInt32 num,
                           epicsUInt32 *numRead,
                           dbCommon *prec,
                           ProcessReason *nextReason = nullptr,
                           epicsUInt32 *statusCode = nullptr,
                           char *statusText = nullptr,
                           const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) = 0;

    /**
     * @brief Write outgoing scalar epicsInt32 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  set to value
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeScalar(const epicsInt32 &value,
                             dbCommon *prec) = 0;

    /**
     * @brief Write outgoing scalar epicsInt64 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  set to value
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeScalar(const epicsInt64 &value,
                             dbCommon *prec) = 0;

    /**
     * @brief Write outgoing scalar epicsUInt32 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  set to value
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeScalar(const epicsUInt32 &value,
                             dbCommon *prec) = 0;

    /**
     * @brief Write outgoing scalar epicsFloat64 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  set to value
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeScalar(const epicsFloat64 &value,
                             dbCommon *prec) = 0;

    /**
     * @brief Write outgoing classic C string (char[]) data.
     *
     * @param value  pointer to null terminated source string
     * @param len  source string length
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeScalar(const char *value,
                             const epicsUInt32 len,
                             dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsInt8 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsInt8 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsUInt8 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsUInt8 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsInt16 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsInt16 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsUInt16 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsUInt16 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsInt32 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsInt32 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsUInt32 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsUInt32 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsInt64 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsInt64 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsUInt64 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsUInt64 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsFloat32 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsFloat32 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of epicsFloat64 data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const epicsFloat64 *value,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Write outgoing array of EPICS String (char[MAX_STRING_SIZE]) data.
     *
     * Takes the provided data value, converts it to the target OPC UA type,
     * sets the record's STAT/SEVR according to the success of the conversion,
     * and triggers an OPC UA write operation.
     *
     * @param value  pointer to source array
     * @param len  length of an EPICS String element
     * @param num  source array size
     * @param prec  pointer to EPICS record
     *
     * @return status  0 = ok, 1 = error
     */
    virtual long writeArray(const char *value,
                            const epicsUInt32 len,
                            const epicsUInt32 num,
                            dbCommon *prec) = 0;

    /**
     * @brief Create processing requests for record(s) attached to this element.
     */
    virtual void requestRecordProcessing(const ProcessReason reason) const = 0;

    const EnumChoices* enumChoices = nullptr;   /**< enum definition if this element is an enum */

protected:
    RecordConnector *pconnector = nullptr;
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENT_H
