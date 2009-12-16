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

#ifndef _CONDOR_STATE_H
#define _CONDOR_STATE_H

// Note: This should be kept in sync with view_server.h and view_server.C and
// with ResConvStr of condor_tools/stats.C.
// Also note that if you add a state, you should update
// _machine_max_state to match
enum State { no_state=0, owner_state, unclaimed_state, matched_state, 
			 claimed_state, preempting_state, shutdown_state,
			 delete_state, backfill_state,
			 _machine_max_state = backfill_state,
			 _state_threshold_, _error_state_ }; 

enum Activity { no_act=0, idle_act, busy_act, retiring_act, vacating_act,
				suspended_act, benchmarking_act, killing_act,
				_act_threshold_, _error_act_ };

const char* state_to_string(State);
State string_to_state(const char*);
const char* activity_to_string(Activity);
Activity string_to_activity(const char*);

#endif /* _CONDOR_STATE_H */



