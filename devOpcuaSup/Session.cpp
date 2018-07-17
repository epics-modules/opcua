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

#include "Session.h"

namespace DevOpcua {

Session::Session (const int debug)
    : debug(debug)
{}

//TODO: Session::findSession() and Session::sessionExists() are currently implemented
// in the (only) implementation SessionUaSdk.
// This should be made dynamic (the session class should manage its implementations).

} // namespace DevOpcua
