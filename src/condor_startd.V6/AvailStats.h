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
