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

   stats_entry_recent<int> Autoclusters;		 // number of active autoclusters

   stats_entry_recent<time_t> JobsAccumTimeToStart; // sum of all time jobs spent waiting to start
   stats_entry_recent<time_t> JobsAccumRunningTime; // sum of all time jobs spent running.
   stats_entry_recent<time_t> JobsAccumBadputTime;  // sum of all time jobs spent running badput
   stats_entry_recent<time_t> JobsAccumExecuteTime;  // sum of all time jobs spent executing the user code ((reap - start) - (pre + post))
   stats_entry_recent<time_t> JobsAccumPreExecuteTime;  // sum of all time jobs spent transferring input   (max(0, exec - start))
   stats_entry_recent<time_t> JobsAccumPostExecuteTime; // sum of all time jobs spent transferring output  (max(0, reap - post))

   // these are to help debug why sum of Execution time doesn't equal sum of running time and badput time.
   stats_entry_recent<time_t> JobsAccumExecuteAltTime;  // sum of all time jobs spent executing the user code, max(0, (post - exec))
   stats_entry_recent<time_t> JobsAccumChurnTime;      // sum of all time shadows wasted with jobs that never produced either goodput or badput
   stats_entry_recent<int>    JobsWierdTimestamps;    // jobs which exited with questionable times for exec or post-exec

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
   int    RecentWindowQuantum;
   int    PublishFlags;

   StatisticsPool          Pool;          // pool of statistics probes and Publish attrib names

   // methods
   //
   void Init(int fOtherPool);
   void Clear();
   time_t Tick(time_t now=0); // call this when time may have changed to update StatsUpdateTime, etc.
   void Reconfig();
   void SetWindowSize(int window);
   void Publish(ClassAd & ad) const;
   void Publish(ClassAd & ad, int flags) const;
   void Publish(ClassAd & ad, const char * config) const;
   //void Unpublish(ClassAd & ad) const;

   } ScheddStatistics;

// this class is used by ScheddOtherStatsMgr to hold a schedd statistics set
class ScheddOtherStats {
public:
   ScheddOtherStats();
   ~ScheddOtherStats();

   ScheddOtherStats * next;	// used to temporarily build a linked list of stats matching a ClassAd
   ScheddStatistics stats;
   MyString     prefix;
   MyString     trigger;
   ExprTree *   trigger_expr;
   bool         enabled;
   bool         by; // when true, there is a set of stats for each unique value of trigger_expr
   time_t       last_match_time; // last time this stats Matched a jobad (seconds)
   time_t       lifetime;        // how long to keep stats after last match (seconds)
   std::map<std::string, ScheddOtherStats*> sets;  // when by==true, holds sets of stats by trigger value
};

class ScheddOtherStatsMgr {
public:
   ScheddOtherStatsMgr() 
     : pools(10, MyStringHash, updateDuplicateKeys)
   {};

   void Clear();
   time_t Tick(time_t now=0); // call this when time may have changed to update StatsUpdateTime, etc.
   void Reconfig();
   void SetWindowSize(int window);
   void Publish(ClassAd & ad);
   void Publish(ClassAd & ad, int flags);
   void Publish(ClassAd & ad, const char * config);
   void UnpublishDisabled(ClassAd & ad);

   // add an entry in the pools hash (by pre), if by==true, also add an entry in the by hash
   bool Enable(const char * pre, const char * trig, bool stats_by_trig_value=false, time_t lifetime=0);
   bool DisableAll();
   bool RemoveDisabled();
   bool AnyEnabled();
   void DeferJobsSubmitted(int cluster, int proc);
   void CountJobsSubmitted(); // finish deferred counting of submitted jobs.

   // returns a linked list of matches, and sets the last_incr_time of each to updateTime
   ScheddOtherStats * Matches(ClassAd & ad, time_t updateTime);

private:
   HashTable<MyString, ScheddOtherStats*> pools; // pools of stats and triggers (for Enable)
   std::map<int,int> deferred_jobs_submitted; // key=cluster, value=max_proc.
};

#endif /* _SCHEDD_STATS_H */
