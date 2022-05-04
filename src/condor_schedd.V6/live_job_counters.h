/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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

#ifndef _LIVE_JOB_COUNTERS_H
#define _LIVE_JOB_COUNTERS_H

// The schedd will have one of these structures per owner, and one for the schedd as a whole
// these counters are new for 8.7, and used with the code that keeps live counts of jobs
// by tracking state transitions
//
struct LiveJobCounters {
  int JobsSuspended;
  int JobsIdle;             // does not count Local or Scheduler universe jobs, or Grid jobs that are externally managed.
  int JobsRunning;
  int JobsRemoved;
  int JobsCompleted;
  int JobsHeld;
  int SchedulerJobsIdle;
  int SchedulerJobsRunning;
  int SchedulerJobsRemoved;
  int SchedulerJobsCompleted;
  int SchedulerJobsHeld;
  void clear_counters() { memset(this, 0, sizeof(*this)); }
  void publish(ClassAd & ad, const char * prefix) const;
  LiveJobCounters()
	: JobsSuspended(0)
	, JobsIdle(0)
	, JobsRunning(0)
	, JobsRemoved(0)
	, JobsCompleted(0)
	, JobsHeld(0)
	, SchedulerJobsIdle(0)
	, SchedulerJobsRunning(0)
	, SchedulerJobsRemoved(0)
	, SchedulerJobsCompleted(0)
	, SchedulerJobsHeld(0)
  {}
};

void IncrementLiveJobCounter(LiveJobCounters & num, int universe, int status, int increment /*, JobQueueJob * job*/);

#endif
