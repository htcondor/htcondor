/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef TIME_OFFSET_H
#define TIME_OFFSET_H

#include "condor_common.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

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

//
// Cedar Specific Funtions
//
long time_offset_cedar_stub( Stream* );
int time_offset_receive_cedar_stub( Service*, int, Stream* );
int time_offset_codePacket_cedar( TimeOffsetPacket&, Stream* );

//
// Logic Functions
//
int time_offset_receive( TimeOffsetPacket& );
long time_offset_calculate( TimeOffsetPacket&, TimeOffsetPacket& );
TimeOffsetPacket time_offset_initPacket( );

#endif // TIME_OFFSET_H
