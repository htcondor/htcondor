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

#include "condor_common.h"
#include "startd.h"

AvailStats::AvailStats()
{
	as_start_avail = 0;
	as_avail_estimate = 0;
	as_avail_confidence = 0.8;
	as_last_avail_interval = 0;
	as_avail_time = 0.0;
	as_num_avail_periods = 0;
	as_max_avail_periods = 100;
	as_tot_avail_time = 0;
	as_birthdate = time(0)-1;	// subtract 1 sec. to avoid div by zero
}

AvailStats::~AvailStats()
{
		// If we're being de-allocated, mark the end of the current 
		// period of availability (if any) and write a checkpoint.
	if( as_start_avail ) {
		update( owner_state, idle_act ); // includes a checkpoint
	} else {
		checkpoint();
	}
}

void
AvailStats::update( State new_state, Activity new_act )
{
	if( !compute_avail_stats ) return;
	if( new_state == owner_state ) {
		if( as_start_avail ) {
				// end of an available period, so insert the duration
				// of this period into the list in sort order
				// (this would be easier with an STL multiset, but
				// STL isn't supported well by gcc yet)
			as_last_avail_interval = time(0) - as_start_avail;
			as_avail_periods.Rewind();
			int item;
			bool inserted = false;
			while (as_avail_periods.Next(item)) {
				if (as_last_avail_interval < item) {
					as_avail_periods.Insert(as_last_avail_interval);
					inserted = true;
					break;
				}
			}
			if (!inserted) {
				as_avail_periods.Append(as_last_avail_interval);
			}
			as_tot_avail_time += as_last_avail_interval;
			as_num_avail_periods++;
			as_start_avail = 0;
			as_avail_estimate = 0;
		}
			// checkpoint our state to disk
		checkpoint();
	} else if( !as_start_avail ) {
		as_start_avail = time(0); // start of new available period
		if( as_num_avail_periods >= as_max_avail_periods ) {
				// if the set is at our maximum configured size, then
				// collapse it by removing every other record
			as_avail_periods.Rewind();
			while( as_avail_periods.Next() ) {
				as_avail_periods.DeleteCurrent();
				as_num_avail_periods--;
				if( as_avail_periods.AtEnd() ) break;
				as_avail_periods.Next();
			}
		}
			// initialize our avail estimate immediately
		compute( A_UPDATE | A_SHARED );

			// checkpoint our state to disk
		checkpoint();
	}
}

void
AvailStats::publish( ClassAd* cp, amask_t how_much )
{
	char line[100];

	if( !compute_avail_stats ) return;

	if( IS_UPDATE(how_much) || IS_PUBLIC(how_much) ) {

		sprintf( line, "%s=%0.2f", ATTR_AVAIL_TIME, avail_time() );
		cp->Insert(line); 
  
		sprintf( line, "%s=%d", ATTR_LAST_AVAIL_INTERVAL,
				 last_avail_interval() );
		cp->Insert(line); 
  
		if( as_start_avail ) {

				// only insert these attributes when in non-owner state
			sprintf( line, "%s=%ld", ATTR_AVAIL_SINCE, (long)avail_since() );
			cp->Insert(line); 
  
			sprintf( line, "%s=%d", ATTR_AVAIL_TIME_ESTIMATE,
					 avail_estimate() );
			cp->Insert(line); 
		}
	}  
}

void
AvailStats::compute( amask_t how_much )
{
	if( !compute_avail_stats ) return;
	if( IS_STATIC(how_much) && IS_SHARED(how_much) ) {

		char *tmp = param("STARTD_AVAIL_CONFIDENCE");
		if( tmp ) {
			as_avail_confidence = atof(tmp);
			if( as_avail_confidence < 0.0 || as_avail_confidence > 1.0 ) {
				as_avail_confidence = 0.8;
			}
			free(tmp);
		}

		tmp = param("STARTD_MAX_AVAIL_PERIOD_SAMPLES");
		if( tmp ) {
			as_max_avail_periods = atoi(tmp);
			free(tmp);
		}
	}

	if( IS_UPDATE(how_much) && IS_SHARED(how_much) ) {
		time_t current_time = time(0);

			// only compute avail time estimate if we're in non-owner state
		if( as_start_avail ) {

				// first, skip over intervals less than our idle time so far
			int current_idle_time = current_time - as_start_avail;
			int num_intervals = as_num_avail_periods;
			as_avail_periods.Rewind();
			int item;
			as_avail_periods.Next(item);
			while( num_intervals && item < current_idle_time ) { 
				as_avail_periods.Next(item);
				num_intervals--;
			}

			if( !num_intervals ) {
					// If this is the longest we've ever been idle, our
					// historical data isn't too useful to us, so give an
					// estimate based on how long we've been idle so far.
				as_avail_estimate =
					(int)floor(current_idle_time*(2.0-as_avail_confidence));
			} else {
					// Otherwise, find the record in our history at the
					// requested confidence level.
				int idx = (int)floor(num_intervals*(1.0-as_avail_confidence));
				while( idx ) {
					as_avail_periods.Next(item);
					idx--;
				}
				as_avail_estimate = item;
			}
			as_avail_time =
				float(as_tot_avail_time + current_idle_time) /
				float(current_time - as_birthdate);
		} else {
			as_avail_time =	float(as_tot_avail_time) /
							float(current_time - as_birthdate);
		}
	}
}

void
AvailStats::serialize( MyString state )
{
	char *s = (char *)state.Value();

	int prev_time = strtol(s, &s, 0);
	as_birthdate -= prev_time;
	int prev_avail_time = strtol(s, &s, 0);
	as_tot_avail_time += prev_avail_time;
	int last_avail_interval = strtol(s, &s, 0);
	if( !as_last_avail_interval ) {
		as_last_avail_interval = last_avail_interval;
	}
	while( 1 ) {
		char *new_s;
		int interval = strtol(s, &new_s, 0);
		if (s == new_s) break;
		s = new_s;
		as_avail_periods.Rewind();
		int item;
		bool inserted = false;
		while (as_avail_periods.Next(item)) {
			if (interval < item) {
				as_avail_periods.Insert(interval);
				inserted = true;
				break;
			}
		}
		if (!inserted) {
			as_avail_periods.Append(interval);
		}
		as_num_avail_periods++;
	}
}

MyString
AvailStats::serialize()
{
	MyString state;
	char buf[20];

	sprintf(buf, "%ld", (long)(time(0)-as_birthdate));
	state += buf;
	sprintf(buf, " %d", as_tot_avail_time);
	state += buf;
	sprintf(buf, " %d", as_last_avail_interval);
	state += buf;
	as_avail_periods.Rewind();
	int item;
	while( as_avail_periods.Next(item) ) {
		sprintf(buf, " %d", item);
		state += buf;
	}

	return state;
}

void
AvailStats::checkpoint_filename( MyString filename )
{
	ckpt_filename = filename;
	tmp_ckpt_filename = ckpt_filename + "tmp";

	FILE *fp = fopen(ckpt_filename.Value(), "r");
	if( fp ) {
		MyString state;
		char buf[1025];
		memset(buf, 0, 1025);
		while( fread(buf, sizeof(char), 1024, fp) ) {
			state += buf;
		}
		fclose(fp);
		serialize(state);
	}
}

void
AvailStats::checkpoint()
{
		// Checkpoint our state to disk by serializing to a string
		// and writing the string to disk.  It's not very efficient
		// to create this string each time, but it shouldn't be too big
		// (under 1KB), so I don't think it's worth worrying too much
		// about efficiency.
	if( ckpt_filename.Length() ) {
		FILE *fp = fopen(tmp_ckpt_filename.Value(), "w");
		if( fp ) {
			MyString state = serialize();
			if( (int)fwrite(state.Value(), sizeof(char), state.Length(),
							fp) == state.Length() ) {
				fclose(fp);
				rotate_file(tmp_ckpt_filename.Value(),
							ckpt_filename.Value());
			} else {
				fclose(fp);
			}
		}
	}
}
