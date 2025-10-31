/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef _RES_STATE_H
#define _RES_STATE_H

#include "condor_state.h"

class ResState
{
public:
	ResState(Resource* rip);
	State	state( void ) const { return r_state; };
	Activity activity( void ) const { return r_act; };
	void	publish( ClassAd* );

	void	change(State new_state) {
		Activity new_act = idle_act;
		if (new_state == preempting_state) { new_act = _preempting_activity(); }
		change(new_state, new_act);
	}
	void	change(Activity new_act) { change( r_state, new_act ); }
	void	change( State, Activity );
	void	revert(State, Activity);

	// evaluate policy expressions like PREEMPT, etc and change state
	// as needed, or do periodic actions.
	int 	eval_policy_periodic( void );
	void	do_periodic_actions();

	struct suggest {
		State _state;
		Activity _act;
		std::string _reason;
		suggest(ResState & that) : _state(that.r_state), _act(that.r_act) {};
	};
	int eval_policy(suggest & sug) {
		return eval_policy(sug._state, sug._act, rip, sug._reason);
	}
	void change(suggest & sug, int xa_code) {
		change_to_suggested_state(sug._state, sug._act, sug._reason, xa_code);
	}

	void	set_destination( State );
	int		starterExited( void );
	State	destination( void ) const { return r_destination; };
	time_t  activityTimeElapsed() const;
	time_t  timeDrainingUnclaimed();

	void	dprintf( int, const char*, ... );
private:
	Resource*	rip;
	State 		r_state;
	State 		r_destination;
	Activity	r_act;		
	bool		r_act_was_benchmark;

	int		enter_action( State, Activity, bool, bool );
	int		leave_action( State cur_s, Activity cur_a, 
						  State new_s, Activity new_a, 
						  bool statechange );

	Activity	_preempting_activity();

	// return enum from eval_policy
	// some of these will trigger special behaviors in change_to_suggested_state
	enum {
		xa_simple = 1,            // simple transistion, no special action
		xa_retirement_expired,    // claim retirement ended/expired - calulate badput
		xa_retirement_expired2,   // claim retirement ended/expired - calulate badput and set preempt is true
		xa_preempt,               // setBadputCausedByPreemption
		xa_preempt_and_retire,    // call rip->retire_claim()
		xa_preempt_may_vacate,    // check and maybe set a vacate reason
		xa_suspend,               // SUSPEND is TRUE
		xa_continue,              // CONTINUE is TRUE
		xa_preclaim_vacate,       // retiring due to preempting claim
		xa_preclaim,              // preempting an idle claim
		xa_false_start,           // START is false
		xa_end_of_worklife,       // idle claim shutting down due to CLAIM_WORKLIFE
		xa_idle_drain,            // idle claim shutting down due to draining of this slot
		xa_killing_time,          // KILL is TRUE
		xa_owner,                 // IS_OWNER is TRUE
		xa_backfill,              // START_BACKFILL is TRUE
		xa_draining,              // entering Drained state
		xa_evict_backfill,        // EVICT_BACKFILL is TRUE
		xa_stopped_drain,         // slot is no longer draining (because it was cancelled)
		xa_drain_complete,        // slot is no longer draining (because it completed)
	};

	// evaluate policy expression like PREEMPT and return what state we should be in
	// with a code to indicate other actions that should be taken before the state change
	static int eval_policy(State & new_state, Activity & new_act, const Resource * rip, std::string & reason);
	void 	change_to_suggested_state(State new_state, Activity new_activity, const std::string & reason, int xa_code);

	time_t	m_stime;	// time we entered the current state
	time_t	m_atime;	// time we entered the current activitiy

		// Timestamps for history statistics
	time_t	m_time_owner_idle;
	time_t	m_time_unclaimed_idle;
	time_t	m_time_unclaimed_benchmarking;
	time_t	m_time_matched_idle;
	time_t	m_time_claimed_idle;
	time_t	m_time_claimed_busy;
	time_t	m_time_claimed_suspended;
	time_t	m_time_claimed_retiring;
	time_t	m_time_preempting_vacating;
	time_t	m_time_preempting_killing;
	time_t	m_time_backfill_idle;
	time_t	m_time_backfill_busy;
	time_t	m_time_backfill_killing;
	time_t  m_time_drained_retiring;
	time_t  m_time_drained_idle;
	time_t  m_time_draining_unclaimed;

	double m_num_cpus_avg;
	double m_draining_avg;
	time_t m_activity_avg_last_timestamp;
	time_t m_activity_avg_time_sum;

	struct HistoryInfo {
		time_t*		time_ptr;
		const char*	attr_name;
	};

	void	updateHistoryTotals( time_t now );
	time_t*		getHistoryTotalPtr( State _state, Activity _act );
	const char*	getHistoryTotalAttr( State _state, Activity _act );
	HistoryInfo	getHistoryInfo( State _state, Activity _act );
	bool	publishHistoryInfo( ClassAd* cap, State _state, Activity _act );
	void resetActivityAverages();
	void updateActivityAverages();
};

#endif /* _RES_STATE_H */

