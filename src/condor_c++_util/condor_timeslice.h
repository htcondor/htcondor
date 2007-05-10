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
		m_last_duration = 0;
		m_next_start_time = 0;
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

	void setStartTimeNow() { m_start_time.getTime(); }
	const UtcTime &getStartTime() const { return m_start_time; }

	void setFinishTimeNow() {
		UtcTime finish_time;
		finish_time.getTime();
		m_last_duration = finish_time.difference(&m_start_time);
		updateNextStartTime();
	}

	void updateNextStartTime() {
		double delay = m_default_interval;
		if( m_start_time.seconds() == 0 ) {
				// never ran before
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
		m_next_start_time = (time_t)floor(
          delay +
		  m_start_time.seconds() +
          m_start_time.microseconds()/1000000 +
          0.5 );
	}

	time_t getNextStartTime() const { return m_next_start_time; }

	int getTimeToNextRun() const { return m_next_start_time - time(NULL); }

	bool isTimeToRun() const { return getTimeToNextRun() <= 0; }

 private:
	double m_timeslice;        // maximum fraction of time to consume
	double m_min_interval;     // minimum delay between runs
	double m_max_interval;     // maximum delay between runs
	double m_default_interval; // default delay between runs
	UtcTime m_start_time;      // when we last started running
	double m_last_duration;    // how long it took to run last time
	time_t m_next_start_time;  // utc second number when to run next time
};

#endif
