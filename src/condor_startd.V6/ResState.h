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

	int 	eval_policy( void ); // evaluate policy expressions like PREEMPT, etc
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
	int m_activity_avg_time_sum;

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

