/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/*
  	This file defines the AvailStats class, which computes statistics
  	about the availability of startd resources.  Each CpuAttributes
  	object contains an AvailStats object.
*/

#ifndef _AVAIL_STATS_H
#define _AVAIL_STATS_H

class AvailStats
{
public:
	AvailStats();
	~AvailStats();

		// update stats on startd vm state change
	void update( State new_state, Activity new_act );

	void publish( ClassAd*, amask_t ); // publish avail stats to given CA
	void compute( amask_t );	// recompute avail stats

	time_t avail_since() { return as_start_avail; }
	float avail_time() { return as_avail_time; }
	int last_avail_interval() { return as_last_avail_interval; }
	int avail_estimate() { return as_avail_estimate; }

	void serialize( MyString state );	// restore state from string
	MyString serialize();	// save state to string

		// specify a filename to use for persistent storage (and load
		// any existing data in that checkpoint file)
	void checkpoint_filename( MyString filename );

		// write a checkpoint to disk
	void checkpoint();

private:
	time_t			as_start_avail;
	int				as_avail_estimate;
	float			as_avail_confidence;
	List<int>		as_avail_periods;
	int				as_last_avail_interval;
	float			as_avail_time;
	int				as_num_avail_periods;
	int				as_max_avail_periods;
	int				as_tot_avail_time;
	time_t			as_birthdate;
	MyString		ckpt_filename, tmp_ckpt_filename;
};

#endif /* _AVAIL_STATS_H */
