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

Timeslice - a class for adapting timing intervals based on how long
previous calls to a function took to run.

Example:

Timeslice timeslice;
timeslice.setTimeslice( 0.1 );       // do not run more than 10% of the time
timeslice.setDefaultInterval( 300 ); // try to run once every 5 minutes
timeslice.setMaxInterval( 3600 );    // run at least hourly

Then either pass the timeslice to daemonCore's Register_Timer(), or
(if you aren't dealing with a timer) do something like the following:

Before each run, call timeslice.setStartTimeNow().
After each run, call timeslice.setFinishTimeNow().

The following functions can then be used to query the timer:

  timeslice.getTimeToNextRun()  // number of seconds until ready to run again
  timeslice.getNextStartTime()  // absolute time of next run
  timeslice.isTimeToRun()       // true if time to next run has passed

*/

#ifndef _CONDOR_TIMESLICE_H_
#define _CONDOR_TIMESLICE_H_

#include "utc_time.h"
#include "math.h"
#include "time.h"

class Timeslice {
 public:
	Timeslice() {
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

	double getLastDuration() const { return m_last_duration; }

	double getTimeslice() const { return m_timeslice; }
	void setTimeslice(double timeslice) {
		m_timeslice = timeslice;
		updateNextStartTime();
	}

	double getMinInterval() const { return m_min_interval; }
	void setMinInterval(double min_interval) {
		m_min_interval = min_interval;
		updateNextStartTime();
	}

	double getMaxInterval() const { return m_max_interval; }
	void setMaxInterval(double max_interval) {
		m_max_interval = max_interval;
		updateNextStartTime();
	}

	double getDefaultInterval() const { return m_default_interval; }
	void setDefaultInterval(double default_interval) {
		m_default_interval = default_interval;
		updateNextStartTime();
	}

	double getInitialInterval() const { return m_initial_interval; }
	void setInitialInterval(double initial_interval) {
		m_initial_interval = initial_interval;
		updateNextStartTime();
	}

	void setStartTimeNow() { m_start_time.getTime(); }
	const UtcTime &getStartTime() const { return m_start_time; }

	void setFinishTimeNow() {
		UtcTime finish_time;
		finish_time.getTime();
		m_last_duration = finish_time.difference(&m_start_time);
		if( m_never_ran_before ) {
			m_avg_duration = m_last_duration;
		}
		else {
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

	void reset() {
		m_last_duration = 0;
		m_start_time = UtcTime();
		m_never_ran_before = true;
		m_expedite_next_run = false;
		updateNextStartTime();
	}

	void updateNextStartTime() {
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
		m_next_start_time = (time_t)floor(
          delay +
		  m_start_time.seconds() +
          m_start_time.microseconds()/1000000.0 +
          0.5 /*round to nearest integer*/ );
	}

	time_t getNextStartTime() const { return m_next_start_time; }

	unsigned int getTimeToNextRun() const {
		int delta = int(m_next_start_time - time(NULL));
		if( delta < 0 ) {
			return 0;
		}
		return (unsigned int)delta;
	}

	bool isTimeToRun() const { return getTimeToNextRun() <= 0; }

	// When computing time til next run, ignore the default interval.
	// The next interval will be as small as is allowed by the
	// min interval and max timeslice.
	void expediteNextRun() {
		m_expedite_next_run = true;
		updateNextStartTime();
	}

 private:
	double m_timeslice;        // maximum fraction of time to consume
	double m_min_interval;     // minimum delay between runs
	double m_max_interval;     // maximum delay between runs
	double m_default_interval; // default delay between runs
	double m_initial_interval; // delay before first run
	UtcTime m_start_time;      // when we last started running
	double m_last_duration;    // how long it took to run last time
	double m_avg_duration;     // moving average duration
	time_t m_next_start_time;  // utc second number when to run next time
	bool m_never_ran_before;   // true if never ran before
	bool m_expedite_next_run;  // true if should ignore default interval
};

// Use this class to set the start/stop times on a timeslice object
// automatically when this object is created and when it goes out of scope.
// Assumes that the timeslice object does not go out of scope before this
// object.
class ScopedTimesliceStopwatch {
 public:
	ScopedTimesliceStopwatch(Timeslice *timeslice):
		m_timeslice( timeslice )
	{
		m_timeslice->setStartTimeNow();
	}
	~ScopedTimesliceStopwatch() {
		m_timeslice->setFinishTimeNow();
	}
 private:
	Timeslice *m_timeslice;
};

#endif
