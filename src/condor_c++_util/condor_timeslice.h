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
		m_next_start_time = 0;
		m_never_ran_before = true;
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
		m_never_ran_before = false;
		updateNextStartTime();
	}

	void reset() {
		m_last_duration = 0;
		m_start_time = UtcTime();
		m_never_ran_before = true;
		updateNextStartTime();
	}

	void updateNextStartTime() {
		double delay = m_default_interval;
		if( m_start_time.seconds() == 0 ) {
				// never got here before
			setStartTimeNow();
		}
		else if( m_timeslice > 0 ) {
			delay = m_last_duration/m_timeslice;
		}
		if( m_max_interval > 0 && delay > m_max_interval ) {
			delay = m_max_interval;
		}
		if( delay < m_min_interval ) {
			delay = m_min_interval;
		}
		if( m_never_ran_before && m_initial_interval >= 0 ) {
			delay = m_initial_interval;
		}
		m_next_start_time = (time_t)floor(
          delay +
		  m_start_time.seconds() +
          m_start_time.microseconds()/1000000 +
          0.5 );
	}

	time_t getNextStartTime() const { return m_next_start_time; }

	int getTimeToNextRun() const { return int(m_next_start_time - time(NULL)); }

	bool isTimeToRun() const { return getTimeToNextRun() <= 0; }

 private:
	double m_timeslice;        // maximum fraction of time to consume
	double m_min_interval;     // minimum delay between runs
	double m_max_interval;     // maximum delay between runs
	double m_default_interval; // default delay between runs
	double m_initial_interval; // delay before first run
	UtcTime m_start_time;      // when we last started running
	double m_last_duration;    // how long it took to run last time
	time_t m_next_start_time;  // utc second number when to run next time
	bool m_never_ran_before;   // true if never ran before
};

#endif
