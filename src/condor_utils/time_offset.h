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


#ifndef TIME_OFFSET_H
#define TIME_OFFSET_H

#include "condor_common.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"

//
// If we fail for whatever reason to figure out what the
// offset is between two daemons, use this value
// I can't think of anytime when the default shouldn't be zero,
// but this just makes for cleaner code this way
//
#define TIME_OFFSET_DEFAULT 0

//
// This struct is just a container for how we will determine
// the offset value.
//
typedef struct TimeOffsetPacket {
	time_t localDepart;
	time_t remoteArrive;
	time_t remoteDepart;
	time_t localArrive;
} TimeOffsetPacket;

// --------------------------------------------
// Cedar Specific Funtions
// --------------------------------------------
bool time_offset_cedar_stub( Stream*, long& );
bool time_offset_range_cedar_stub( Stream*, long&, long& );

//
// These methods are generic and could be used for the range
// calculation or collecting offset information
//
bool time_offset_send_cedar_stub( Stream*, TimeOffsetPacket&, TimeOffsetPacket& );
int time_offset_receive_cedar_stub(int, Stream* );
bool time_offset_codePacket_cedar( TimeOffsetPacket&, Stream* );

// --------------------------------------------
// Logic Functions
// --------------------------------------------
bool time_offset_receive( TimeOffsetPacket& );
bool time_offset_validate( TimeOffsetPacket&, TimeOffsetPacket& );
bool time_offset_calculate( TimeOffsetPacket&, TimeOffsetPacket&, long& );
bool time_offset_range_calculate( TimeOffsetPacket&, TimeOffsetPacket&, long&, long& );
TimeOffsetPacket time_offset_initPacket( );

#endif // TIME_OFFSET_H
