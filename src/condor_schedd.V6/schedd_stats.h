/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef _SCHEDD_STATS_H
#define _SCHEDD_STATS_H

#include <generic_stats.h>

// the windowed schedd statistics are quantized to the nearest N seconds
// STATISTICS_WINDOW_SECONDS/schedd_stats_window_quantum is the number of slots
// in the window ring_buffer.
const int schedd_stats_window_quantum = 200;

// this struct is used to contain statistics values for the Scheduler class.
// the values are published using the names as shown here. so for instance
// the ClassAd that we publish into will have "JobsSubmitted=32" if the
// JobsSubmitted field below contains the value 32. 
//
typedef struct ScheddStatistics {

   // these are used by generic tick
   time_t StatsLifetime;         // the total time covered by this set of statistics
   time_t StatsLastUpdateTime;   // last time that statistics were last updated. (a freshness time)
   time_t RecentStatsLifetime;   // actual time span of current RecentXXX data.
   time_t RecentStatsTickTime;   // last time Recent values Advanced

   // basic job counts
   stats_entry_recent<int> JobsSubmitted;        // jobs submitted over lifetime of schedd
   stats_entry_recent<int> JobsStarted;          // jobs started over schedd lifetime
   stats_entry_recent<int> JobsExited;           // jobs exited (success or failure) over schedd lifetime
   stats_entry_recent<int> JobsCompleted;        // jobs successfully completed over schedd lifetime

   stats_entry_recent<time_t> JobsAccumTimeToStart; // sum of all time jobs spent waiting to start
   stats_entry_recent<time_t> JobsAccumRunningTime; // sum of all time jobs spent running.
   stats_entry_recent<time_t> JobsAccumBadputTime;  // sum of all time jobs spent running badput

   // counts of various exit conditions.
   stats_entry_recent<int> JobsExitedNormally; // jobs that exited with JOB_EXITED or JOB_EXITED_AND_CLAIM_CLOSING
   stats_entry_recent<int> JobsKilled;         // jobs that exited with JOB_KILLED, JOB_NO_CKPT_FILE or JOB_SHOULD_REMOVE
   stats_entry_recent<int> JobsExitException;  // jobs that exited with JOB_EXCEPTION, DPRINTF_ERROR, or any unknown code
   stats_entry_recent<int> JobsExecFailed;     // jobs that exited with JOB_EXEC_FAILED or JOB_NO_MEM

   stats_entry_recent<int> JobsShadowNoMemory;        // jobs that exited with JOB_NO_MEM
   stats_entry_recent<int> JobsCheckpointed;          // jobs that exited with JOB_CKPTED
   stats_entry_recent<int> JobsShouldRequeue;         // jobs that exited with JOB_SHOULD_REQUEUE
   stats_entry_recent<int> JobsNotStarted;            // jobs that exited with JOB_NOT_STARTED
   stats_entry_recent<int> JobsShouldRemove;          // jobs that exited with JOB_SHOULD_REMOVE
   stats_entry_recent<int> JobsExitedAndClaimClosing; // jobs that exited with JOB_EXITED_AND_CLAIM_CLOSING
   stats_entry_recent<int> JobsCoredumped;            // jobs that exited with JOB_COREDUMPED
   stats_entry_recent<int> JobsMissedDeferralTime;    // jobs that exited with JOB_MISSED_DEFERRAL_TIME
   stats_entry_recent<int> JobsShouldHold;            // jobs that exited with JOB_SHOULD_HOLD
   stats_entry_recent<int> JobsDebugLogError;         // jobs that exited with DPRINTF_ERROR

   // counts of shadow processes
   stats_entry_abs<int> ShadowsRunning;          // current number of running shadows, also tracks the peak value.
   stats_entry_recent<int> ShadowsStarted;       // number of shadow processes that have been started
   stats_entry_recent<int> ShadowsRecycled;      // number of times shadows have been recycled
   //stats_entry_recent<int> ShadowExceptions;     // number of times shadows have excepted
   stats_entry_recent<int> ShadowsReconnections; // number of times shadows have reconnected

   stats_entry_recent_histogram<int64_t> JobsCompletedSizes;
   stats_entry_recent_histogram<int64_t> JobsBadputSizes;
   stats_entry_recent_histogram<time_t> JobsCompletedRuntimes;
   stats_entry_recent_histogram<time_t> JobsBadputRuntimes;

   stats_histogram<int64_t> JobsRunningSizes;
   stats_histogram<time_t>  JobsRunningRuntimes;

   // non-published values
   time_t InitTime;            // last time we init'ed the structure
   int    RecentWindowMax;     // size of the time window over which RecentXXX values are calculated.
   int    PublishFlags;

   StatisticsPool          Pool;          // pool of statistics probes and Publish attrib names

   // methods
   //
   void Init();
   void Clear();
   time_t Tick(time_t now=0); // call this when time may have changed to update StatsUpdateTime, etc.
   void Reconfig();
   void SetWindowSize(int window);
   void Publish(ClassAd & ad) const;
   void Publish(ClassAd & ad, int flags) const;
   void Publish(ClassAd & ad, const char * config) const;
   void Unpublish(ClassAd & ad) const;

   } ScheddStatistics;


#endif /* _SCHEDD_STATS_H */
