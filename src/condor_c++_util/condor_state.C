#include "condor_common.h"
#include "condor_state.h"

static char *_FileName_ = __FILE__;

static char* condor_states [] = { "Owner", "Unclaimed", "Matched",
								  "Claimed", "Preempting" };

static char* condor_activities [] = { "Idle", "Busy", "Vacating",
									  "Suspended", "Benchmarking",
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


