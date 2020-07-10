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
	Timeslice();

	double getLastDuration() const { return m_last_duration; }

	double getTimeslice() const { return m_timeslice; }
	void setTimeslice(double timeslice);

	double getMinInterval() const { return m_min_interval; }
	void setMinInterval(double min_interval);

	double getMaxInterval() const { return m_max_interval; }
	void setMaxInterval(double max_interval);

	double getDefaultInterval() const { return m_default_interval; }
	void setDefaultInterval(double default_interval);

	double getInitialInterval() const { return m_initial_interval; }
	void setInitialInterval(double initial_interval);

	void setStartTimeNow() { condor_gettimestamp( m_start_time ); }
	const struct timeval &getStartTime() const { return m_start_time; }

	void setFinishTimeNow();

		// instead of calling startFinishTimeNow() followed by setFinishTimeNow(),
		// processEvent() can be passed the start and stop times
	void processEvent(struct timeval start, struct timeval finish);

	void reset();

	void updateNextStartTime();

	time_t getNextStartTime() const { return m_next_start_time; }

	unsigned int getTimeToNextRun() const;

	bool isTimeToRun() const { return getTimeToNextRun() <= 0; }

	// When computing time til next run, ignore the default interval.
	// The next interval will be as small as is allowed by the
	// min interval and max timeslice.
	void expediteNextRun();
	bool isNextRunExpedited() const { return m_expedite_next_run; }

 private:
	double m_timeslice;        // maximum fraction of time to consume
	double m_min_interval;     // minimum delay between runs
	double m_max_interval;     // maximum delay between runs
	double m_default_interval; // default delay between runs
	double m_initial_interval; // delay before first run
	struct timeval m_start_time; // when we last started running
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
