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
#ifndef _RES_STATE_H
#define _RES_STATE_H

#include "condor_state.h"

class ResState
{
public:
	ResState(Resource* rip);
	State	state( void ) { return r_state; };
	Activity activity( void ) { return r_act; };
	void	publish( ClassAd*, amask_t );
	int		change( Activity );
	int		change( State );
	int		change( State, Activity );
	int 	eval( void );
	void	set_destination( State );
	int		starterExited( void );
	State	destination( void ) { return r_destination; };

	void	dprintf( int, char*, ... );
private:
	Resource*	rip;
	State 		r_state;
	State 		r_destination;
	Activity	r_act;		

	int		enter_action( State, Activity, bool, bool );
	int		leave_action( State cur_s, Activity cur_a, 
						  State new_s, Activity new_a, 
						  bool statechange );

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

	struct HistoryInfo {
		time_t*		time_ptr;
		const char*	attr_name;
	};

	void	updateHistoryTotals( time_t now );
	time_t*		getHistoryTotalPtr( State _state, Activity _act );
	const char*	getHistoryTotalAttr( State _state, Activity _act );
	HistoryInfo	getHistoryInfo( State _state, Activity _act );
	bool	publishHistoryInfo( ClassAd* cap, State _state, Activity _act );

};

#endif /* _RES_STATE_H */

