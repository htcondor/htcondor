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

#include "condor_common.h"
#include "condor_state.h"

static char* condor_states [] = 
{ "None", "Owner", "Unclaimed", "Matched",  "Claimed", "Preempting",
  "Shutdown", "Delete", "Backfill" };

static char* condor_activities [] = 
{ "None", "Idle", "Busy", "Retiring", "Vacating", "Suspended", "Benchmarking",
  "Killing" };

State 
string_to_state(const char* state_string) 
{
	int i;
	for( i=0; i<_state_threshold_; i++ ) {
		if( !strcmp(condor_states[i], state_string) ) {
			return (State)i;
		}
	}
	return _error_state_;
}


const char*
state_to_string( State state )
{
	if( state < _state_threshold_ ) {
		return condor_states[state];
	} else {
		return "Unknown";
	}
}


Activity
string_to_activity( const char* act_string ) 
{
	int i;
	for( i=0; i<_act_threshold_; i++ ) {
		if( !strcmp(condor_activities[i], act_string) ) {
			return (Activity)i;
		}
	}
	return _error_act_;
}


const char*
activity_to_string( Activity act )
{
	if( act < _act_threshold_ ) {
		return condor_activities[act];
	} else {
		return "Unknown";
	}
}


