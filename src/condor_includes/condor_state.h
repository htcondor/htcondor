#ifndef _CONDOR_STATE_H
#define _CONDOR_STATE_H

enum State { owner_state, unclaimed_state, matched_state, 
			 claimed_state, preempting_state, _state_threshold_,
			 _error_state_ }; 

enum Activity { idle_act, busy_act, vacating_act, suspended_act, 
				benchmarking_act, killing_act, _act_threshold_,
				_error_act_ };

char* state_to_string(State);
State string_to_state(char*);
char* activity_to_string(Activity);
Activity string_to_activity(char*);

#endif _CONDOR_STATE_H



