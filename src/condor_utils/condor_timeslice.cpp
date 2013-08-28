/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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
Timeslice - a class for adapting timing intervals based on how long
previous calls to a function took to run.

See condor_timeslice.h for how to use it.
*/

#include "condor_common.h"
#include "condor_timeslice.h"

Timeslice::Timeslice() {
		m_timeslice = 0;
		m_min_interval = 0;
		m_max_interval = 0;
		m_default_interval = 0;
		m_initial_interval = -1;
		m_last_duration = 0;
		m_avg_duration = 0;
		m_next_start_time = 0;
		m_never_ran_before = true;
		m_expedite_next_run = true;
	}

void 
Timeslice::setTimeslice(double timeslice) 
{
	m_timeslice = timeslice;
	updateNextStartTime();
}

void 
Timeslice::setMinInterval(double min_interval) {
	m_min_interval = min_interval;
	updateNextStartTime();
}

void 
Timeslice::setMaxInterval(double max_interval) 
{
	m_max_interval = max_interval;
	updateNextStartTime();
}

void 
Timeslice::setDefaultInterval(double default_interval) {
	m_default_interval = default_interval;
	updateNextStartTime();
}

void 
Timeslice::setInitialInterval(double initial_interval) 
{
	m_initial_interval = initial_interval;
	updateNextStartTime();
}


void 
Timeslice::setFinishTimeNow() 
{
	UtcTime finish_time;
	finish_time.getTime();
	processEvent( m_start_time, finish_time );
}
void
Timeslice::processEvent(UtcTime start,UtcTime finish)
{
	m_start_time = start;
	double duration = finish.difference(&start);
	m_last_duration = duration;
	if( m_never_ran_before ) {
		m_avg_duration = m_last_duration;
	} else {
			// Compute the exponential moving average of last_duration.
			// This is intended to smooth over spikes that may happen.
			// alpha --> 0 clings to the past; alpha --> 1 ignores the past
			// Until we have a good reason to make this configurable,
			// alpha=0.4 is simply hard-coded.  It takes roughly 5 cycles
			// to get within 10% of a new radically different value.
		const double alpha = 0.4;
		m_avg_duration = alpha*m_last_duration + (1.0-alpha)*m_avg_duration;
	}
	m_never_ran_before = false;
	m_expedite_next_run = false;
	updateNextStartTime();
}

void 
Timeslice::reset() 
{
	m_last_duration = 0;
	m_start_time = UtcTime();
	m_never_ran_before = true;
	m_expedite_next_run = false;
	updateNextStartTime();
}

void 
Timeslice::updateNextStartTime() 
{
	// Keep in mind in reading the following that "delay" is the
	// time to wait before running again starting from the previous
	// _start_ time (NOT from the previous end time).

	double delay = m_default_interval;
	if( m_expedite_next_run ) {
		// ignore default, run as soon as possible
		// this may be adjusted below by min interval and max timeslice
		delay = 0;
	}
	if( m_start_time.seconds() == 0 ) {
			// there is no previous start time (because this is
			// the first time) so pretend that we just ran, and
			// ignore timeslice delay
		setStartTimeNow();
	}
	else if( m_timeslice > 0 ) {
		// Compute the soonest start time from the previous start time that
		// would result in the desired timeslice fraction, where the
		// timeslice is the amount of time spent running divided by
		// the total time between the start of one run to the next run.
		// A timeslice of 1.0 means we can run right away, because the
		// delay will just be equal to the run time.  A timeslice less
		// than 1.0 means we must delay some amount before running again.

		double slice_delay = m_avg_duration/m_timeslice;
		if( delay < slice_delay ) {
			delay = slice_delay;
		}
	}
	if( m_max_interval > 0 && delay > m_max_interval ) {
		delay = m_max_interval;
	}
	if( delay < m_min_interval ) {
		delay = m_min_interval;
	}
	if( m_never_ran_before && m_initial_interval >= 0 ) {
		// we never ran before and an initial interval was explicitly given
		delay = m_initial_interval;
	}

	if( delay > 0.5 || delay < 0.0 ) {
		m_next_start_time = (time_t)floor(
	      delay +
		  m_start_time.combined() +
	      0.5 /*round to nearest integer*/ );
	}
	else {
			// For delays less than 0.5s, we have to deal with the
			// fact that the next start time is effectively either
			// *now* or time(NULL)+1, assuming m_start_time is now or
			// earlier.  Simply rounding to the nearest timestamp
			// produces a delay that is slightly too big on average.
			// This is especially annoying when the delay is supposed
			// to be 0s, because rouding produces an average of 0.125s.

			// The solution is to solve for k, the cuttoff past which
			// we should round up to produce the desired average delay.

			// d = desired delay
			// t = fraction of current second that has already passed
			// k = cutoff past which t should be rounded up

			// Given t, the actual delay is:
			// 0 <= t < k: 0
			// k <= t < 1: 1-t

			// Avg delay = 0.5*(1-k)(1-k) == d

			// k = 1-sqrt(2d)

		double cutoff = 1.0 - sqrt(2.0*delay);
		m_next_start_time = (time_t)m_start_time.seconds();
		if( m_start_time.microseconds()/1000000.0 > cutoff ) {
			m_next_start_time++;
		}
	}
}

unsigned int 
Timeslice::getTimeToNextRun() const 
{
	int delta = int(m_next_start_time - time(NULL));
	if( delta < 0 ) {
		return 0;
	}
	return (unsigned int)delta;
}

	// When computing time til next run, ignore the default interval.
	// The next interval will be as small as is allowed by the
	// min interval and max timeslice.
void 
Timeslice::expediteNextRun() 
{
	m_expedite_next_run = true;
	updateNextStartTime();
}
