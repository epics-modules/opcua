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

#include "Session.h"
#include <uasession.h>

namespace DevOpcua {

using namespace UaClientSdk;

Session::Session()
{
    pSession = new UaSession();
}

Session::~Session()
{
    if (pSession) {
        if (pSession->isConnected() != OpcUa_False) {
            ServiceSettings serviceSettings;
            pSession->disconnect(serviceSettings, OpcUa_True);
        }
        delete pSession;
        pSession = NULL;
    }
}

} // namespace DevOpcua
