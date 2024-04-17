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

// this struct is used to contain statistics values about Jobs for the Scheduler class.
// it is separated out from general Schedd statistics so that we can create an instance
// of this class for each SCHEDD_COLLECT_STATS_BY_* config knob.
//
typedef struct ScheddJobCounters {

   stats_entry_abs<int> JobsRunning; // number of running jobs, counted so that we can do BY_* and FOR_* counters
   stats_histogram<int64_t> JobsRunningSizes;
   stats_histogram<time_t>  JobsRunningRuntimes;
   stats_entry_abs<int> JobsUnmaterialized;

   stats_entry_recent<int> JobsSubmitted;        // jobs submitted over lifetime of schedd
   stats_entry_recent<int> JobsStarted;          // jobs started over schedd lifetime
   stats_entry_recent<int> JobsExited;           // jobs exited (success or failure) over schedd lifetime
   stats_entry_recent<int> JobsCompleted;        // jobs successfully completed over schedd lifetime

   stats_entry_recent_histogram<int64_t> JobsCompletedSizes;
   stats_entry_recent_histogram<int64_t> JobsBadputSizes;
   stats_entry_recent_histogram<time_t> JobsCompletedRuntimes;
   stats_entry_recent_histogram<time_t> JobsBadputRuntimes;

   stats_entry_recent<time_t> JobsAccumTimeToStart; // sum of all time jobs spent waiting to start
   stats_entry_recent<time_t> JobsAccumRunningTime; // sum of all time jobs spent running.
   stats_entry_recent<time_t> JobsAccumBadputTime;  // sum of all time jobs spent running badput
   stats_entry_recent<time_t> JobsAccumExceptionalBadputTime;  // sum of all time jobs spent running badput and the shadow excepted
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
   stats_entry_recent<int> JobsCoolDowns;             // jobs that entered cool-down

   void InitJobCounters(StatisticsPool &Pool, int base_verbosity);

} ScheddJobCounters;

typedef struct ScheddJobStatistics : public ScheddJobCounters {

   StatisticsPool Pool;          // pool of statistics probes and Publish attrib names

   // methods
   //
   void InitOther(int window, int quantum);
   void Clear() { Pool.Clear(); }
   void Advance(int cAdvance) { if (cAdvance) Pool.Advance(cAdvance); }
   void SetWindowSize(int window, int quantum);
   void Publish(ClassAd & ad, int flags) const { Pool.Publish(ad, flags); }

} ScheddJobStatistics;

// this struct is used to contain statistics values for the Scheduler class.
// the values are published using the names as shown here. so for instance
// the ClassAd that we publish into will have "ShadowsRunning=32" if the
// ShadowsRunning field below contains the value 32. 
//
typedef struct ScheddStatistics : public ScheddJobCounters {

   // these are used by generic tick
   time_t StatsLifetime;         // the total time covered by this set of statistics
   time_t StatsLastUpdateTime;   // last time that statistics were last updated. (a freshness time)
   time_t RecentStatsLifetime;   // actual time span of current RecentXXX data.
   time_t RecentStatsTickTime;   // last time Recent values Advanced

   stats_entry_recent<int> Autoclusters;   // number of active autoclusters
   stats_entry_recent<int> ResourceRequestsSent;   // number of resource requests

   // These track how successful the schedd was at reconnecting to
   // running jobs after the last restart.
   // How many reconnect attempts failed.
   stats_entry_abs<int> JobsRestartReconnectsFailed;
   // How many reconnects weren't attempted because the lease was
   // already expired when the schedd started up.
   stats_entry_abs<int> JobsRestartReconnectsLeaseExpired;
   // How many reconnect attmepts succeeded.
   stats_entry_abs<int> JobsRestartReconnectsSucceeded;
   // How many reconnect attempts are currently in progress.
   stats_entry_abs<int> JobsRestartReconnectsAttempting;
   // How many reconnect attempts ended with the shadow exiting without
   // telling the schedd whether the reconnect succeeded.
   stats_entry_abs<int> JobsRestartReconnectsInterrupted;
   // How much cumulative job runtime was lost due to failure to
   // reconnect to running jobs.
   stats_histogram<time_t> JobsRestartReconnectsBadput;

   // counts of shadow processes
   stats_entry_abs<int> ShadowsRunning;          // current number of running shadows, also tracks the peak value.
   stats_entry_recent<int> ShadowsStarted;       // number of shadow processes that have been started
   stats_entry_recent<int> ShadowsRecycled;      // number of times shadows have been recycled
   //stats_entry_recent<int> ShadowExceptions;     // number of times shadows have excepted
   stats_entry_recent<int> ShadowsReconnections; // number of times shadows have reconnected


   // non-published values
   time_t InitTime;            // last time we init'ed the structure
   int    RecentWindowMax;     // size of the time window over which RecentXXX values are calculated.
   int    RecentWindowQuantum;
   int    PublishFlags;
   int    AdvanceAtLastTick;

   StatisticsPool          Pool;          // pool of statistics probes and Publish attrib names

   // methods
   //
   void InitMain();
   void Clear();
   time_t Tick(time_t now=0); // call this when time may have changed to update StatsUpdateTime, etc.
   void Reconfig();
   void SetWindowSize(int window);
   void Publish(ClassAd & ad) const;
   void Publish(ClassAd & ad, int flags) const;
   void Publish(ClassAd & ad, const char * config) const;
   //void Unpublish(ClassAd & ad) const;

   } ScheddStatistics;

// this class is used by ScheddOtherStatsMgr to hold a schedd job statistics set
class ScheddOtherStats {
public:
   ScheddOtherStats();
   ~ScheddOtherStats();

   ScheddOtherStats * next;	// used to temporarily build a linked list of stats matching a ClassAd
   ScheddJobStatistics stats;
   std::string  prefix;
   std::string  trigger;
   ExprTree *   trigger_expr;
   bool         enabled;
   bool         by; // when true, there is a set of stats for each unique value of trigger_expr
   time_t       last_match_time; // last time this stats Matched a jobad (seconds)
   time_t       lifetime;        // how long to keep stats after last match (seconds)
   std::map<std::string, ScheddOtherStats*> sets;  // when by==true, holds sets of stats by trigger value
};

class ScheddOtherStatsMgr {
public:
   ScheddOtherStatsMgr(ScheddStatistics & stats)
     : config(stats)
	 , pools(hashFunction)
   {};
   ~ScheddOtherStatsMgr();

   void Clear();
   time_t Tick(time_t now); // call this when time may have changed to update StatsUpdateTime, etc.
   void Reconfig(int window, int quantum);
   void SetWindowSize(int window, int quantum);
   //void Publish(ClassAd & ad);
   void Publish(ClassAd & ad, int flags);
   //void Publish(ClassAd & ad, const char * config);
   void UnpublishDisabled(ClassAd & ad);

   // add an entry in the pools hash (by pre), if by==true, also add an entry in the by hash
   bool Enable(const char * pre, const char * trig, bool stats_by_trig_value=false, time_t lifetime=0);
   bool Contains(const char * pre) { return pools.exists(pre) == 0; }
   ScheddOtherStats * Lookup(const char * pre) { ScheddOtherStats * po = NULL; pools.lookup(pre, po); return po; }
   bool DisableAll(); // returns true if any were enabled before this call
   bool RemoveDisabled();
   bool AnyEnabled();
   void DeferJobsSubmitted(int cluster, int proc);
   void CountJobsSubmitted(); // finish deferred counting of submitted jobs.
   void ResetJobsRunning(); // reset jobs-running counters/histograms in preparation for count_jobs

   // returns a linked list of matches, and sets the last_incr_time of each to updateTime
   ScheddOtherStats * Matches(ClassAd & ad, time_t updateTime);

private:
   ScheddStatistics & config;  // ref to ScheddStatistics that we pull config from
   HashTable<std::string, ScheddOtherStats*> pools; // pools of stats and triggers (for Enable)
   std::map<int,int> deferred_jobs_submitted; // key=cluster, value=max_proc.
};

class schedd_runtime_probe : public stats_entry_probe<double> {};

#endif /* _SCHEDD_STATS_H */
