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

		// update stats on startd slot state change
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
	SimpleList<int>	as_avail_periods;
	int				as_last_avail_interval;
	float			as_avail_time;
	int				as_num_avail_periods;
	int				as_max_avail_periods;
	int				as_tot_avail_time;
	time_t			as_birthdate;
	MyString		ckpt_filename, tmp_ckpt_filename;
};

#endif /* _AVAIL_STATS_H */
