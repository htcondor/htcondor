/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef _CONDOR_DAEMON_TYPES_H
#define _CONDOR_DAEMON_TYPES_H

#include "condor_adtypes.h"

// if you add another type to this list, make sure to edit
// daemon_types.C and add the string equivilant.

typedef enum : long {
    DT_NONE, DT_ANY,
    DT_MASTER, DT_SCHEDD, DT_STARTD,
    DT_COLLECTOR, DT_NEGOTIATOR, DT_KBDD,
    DT_DAGMAN, DT_VIEW_COLLECTOR, DT_CLUSTER,
    DT_SHADOW, DT_STARTER, DT_CREDD, DT_GRIDMANAGER,
    DT_TRANSFERD, DT_LEASE_MANAGER, DT_HAD,
    DT_GENERIC,
    _dt_threshold_
} daemon_t;

const char* daemonString( daemon_t dt );
daemon_t stringToDaemonType( const char* name );
// convert MyType string from a location ad to the daemon_t enum
daemon_t AdTypeStringToDaemonType( const char* adtype_name );
// convert MyType string to the AdTypes enum
// returns NO_AD for ads that have a valid MyType but no corresponding enum value
AdTypes AdTypeStringToAdType(const char* adtype_name);

// If `d` corresponds to an ad type, sets `a` to that type and return true;
// otherwise, returns false.
bool convert_daemon_type_to_ad_type(daemon_t d, AdTypes & a);

#endif /* _CONDOR_DAEMON_TYPES_H */
