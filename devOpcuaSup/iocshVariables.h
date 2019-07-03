/*************************************************************************\
* Copyright (c) 2018-2019 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 */

#ifndef DEVOPCUA_IOCSHVARIABLES_H
#define DEVOPCUA_IOCSHVARIABLES_H

namespace DevOpcua {

/**
 * @brief Default settings for configuration options.
 *
 * These variables can be set from the iocShell's st.cmd script to define default
 * values for several configuration options.
 * They can be set between loading data files to adapt defaults to specific
 * applications.
 */

// session
extern double opcua_ConnectTimeout;            /**< connect timeout / reconnect attempt interval [s] */
extern int opcua_MaxOperationsPerServiceCall;  /**< batch size for operations (0 = no limit, don't batch) */

// subscription
extern double opcua_DefaultPublishInterval;    /**< publishing interval [ms] */

// monitored item
extern double opcua_DefaultSamplingInterval;   /**< sampling interval [ms] (-1 = use publishing interval) */
extern int opcua_DefaultServerQueueSize;       /**< server side queue size (1 = no queuing) */
extern int opcua_DefaultDiscardOldest;         /**< discard policy on queue overrun (1 = discard oldest; 0 = newest) */
extern int opcua_DefaultUseServerTime;         /**< timestamp selection (1 = use server time; 0 use source time) */
extern int opcua_DefaultOutputReadback;        /**< output record handling (1 = bidirectional) */
extern double opcua_ClientQueueSizeFactor;     /**< client queue size factor (* server side size) */
extern int opcua_MinimumClientQueueSize;       /**< minimum client queue size */

} // namespace DevOpcua

#endif // DEVOPCUA_IOCSHVARIABLES_H
