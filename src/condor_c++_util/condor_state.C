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
#include "condor_common.h"
#include "condor_state.h"

static char* condor_states [] = 
{ "None", "Owner", "Unclaimed", "Matched",  "Claimed", "Preempting",
  "Shutdown", "Delete", "Backfill" };

static char* condor_activities [] = 
{ "None", "Idle", "Busy", "Retiring", "Vacating", "Suspended", "Benchmarking",
  "Killing" };

State 
string_to_state(char* state_string) 
{
	int i;
	for( i=0; i<_state_threshold_; i++ ) {
		if( !strcmp(condor_states[i], state_string) ) {
			return (State)i;
		}
	}
	return _error_state_;
}


char*
state_to_string( State state )
{
	if( state < _state_threshold_ ) {
		return condor_states[state];
	} else {
		return "Unknown";
	}
}


Activity
string_to_activity( char* act_string ) 
{
	int i;
	for( i=0; i<_act_threshold_; i++ ) {
		if( !strcmp(condor_activities[i], act_string) ) {
			return (Activity)i;
		}
	}
	return _error_act_;
}


char*
activity_to_string( Activity act )
{
	if( act < _act_threshold_ ) {
		return condor_activities[act];
	} else {
		return "Unknown";
	}
}


