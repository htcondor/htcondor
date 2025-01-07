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


#include "condor_common.h"
#include "scheduler.h"
#include "schedd_stats.h"
#include "condor_config.h"
#include "classad_helpers.h"
#include "qmgmt.h"

void ScheddStatistics::Reconfig()
{
    int quantum = param_integer("STATISTICS_WINDOW_QUANTUM_SCHEDULER", INT_MAX, 1, INT_MAX);
    if (quantum >= INT_MAX)
        quantum = param_integer("STATISTICS_WINDOW_QUANTUM_SCHEDD", INT_MAX, 1, INT_MAX);
    if (quantum >= INT_MAX)
        quantum = param_integer("STATISTICS_WINDOW_QUANTUM", 4*60, 1, INT_MAX);
    this->RecentWindowQuantum = quantum;

    this->RecentWindowMax = param_integer("STATISTICS_WINDOW_SECONDS", 1200, quantum, INT_MAX);

    this->PublishFlags    = IF_BASICPUB | IF_RECENTPUB;
    char * tmp = param("STATISTICS_TO_PUBLISH");
    if (tmp) {
       this->PublishFlags = generic_stats_ParseConfigString(tmp, "SCHEDD", "SCHEDULER", this->PublishFlags);
       free(tmp);
    }
    SetWindowSize(this->RecentWindowMax);

    std::string strWhitelist;
    if (param(strWhitelist, "STATISTICS_TO_PUBLISH_LIST")) {
       this->Pool.SetVerbosities(strWhitelist.c_str(), this->PublishFlags, true);
    }

    //stats_histogram_sizes::init_sizes_from_param("MAX_HIST_SIZES_LEVELS");
    //JobSizes.reconfig();
    //JobSizesGoodput.reconfig();
    //JobSizesBadput.reconfig();
}

void ScheddStatistics::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   Pool.SetRecentMax(window, MAX(this->RecentWindowQuantum,1));
}


#define SCHEDD_STATS_ADD_RECENT(pool,name,as)  STATS_POOL_ADD_VAL_PUB_RECENT(pool, "", name, as) 
#define SCHEDD_STATS_ADD_VAL(pool,name,as)     STATS_POOL_ADD_VAL(pool, "", name, as) 
#define SCHEDD_STATS_PUB_PEAK(pool,name,as)    STATS_POOL_PUB_PEAK(pool, "", name, as) 
#define SCHEDD_STATS_PUB_DEBUG(pool,name,as)   STATS_POOL_PUB_DEBUG(pool, "", name, as) 

static const int64_t default_job_hist_sizes[] = {
      (int64_t)0x10000 * 0x1,        (int64_t)0x10000 * 0x4,      // 64Kb, 256Kb
      (int64_t)0x10000 * 0x10,       (int64_t)0x10000 * 0x40,     //  1Mb,   4Mb
      (int64_t)0x10000 * 0x100,      (int64_t)0x10000 * 0x400,    // 16Mb,  64Mb
      (int64_t)0x10000 * 0x1000,     (int64_t)0x10000 * 0x4000,   //256Mb,   1Gb  
      (int64_t)0x10000 * 0x10000,    (int64_t)0x10000 * 0x40000,  //  4Gb,  16Gb
      (int64_t)0x10000 * 0x100000,   (int64_t)0x10000 * 0x400000, // 64Gb, 256Gb
      };
static const char default_sizes_set[] = "64Kb, 256Kb, 1Mb, 4Mb, 16Mb, 64Mb, 256Mb, 1Gb, 4Gb, 16Gb, 64Gb, 256Gb";
static const time_t default_job_hist_lifes[] = {
      (time_t)30,            (time_t) 1 * 60,        // 30 Sec,  1 Min
      (time_t) 3 * 60,       (time_t)10 * 60,        //  3 Min, 10 Min,
      (time_t)30 * 60,       (time_t) 1 * 60*60,     // 30 Min,  1 Hr,
      (time_t) 3 * 60*60,    (time_t) 6 * 60*60,     //  3 Hr    6 Hr,
      (time_t)12 * 60*60,    (time_t) 1 * 24*60*60,  // 12 Hr    1 Day,
      (time_t) 2 * 24*60*60, (time_t) 4 * 24*60*60,  //  2 Day   4 Day,
      (time_t) 8 * 24*60*60, (time_t)16 * 24*60*60,  //  8 Day  16 Day,
      };
static const char default_lifes_set[] = "30Sec, 1Min, 3Min, 10Min, 30Min, 1Hr, 3Hr, 6Hr, 12Hr, 1Day, 2Day, 4Day, 8Day, 16Day";

void ScheddJobCounters::InitJobCounters(StatisticsPool &Pool, int base_verbosity)
{
   JobsRunningSizes.set_levels(default_job_hist_sizes, COUNTOF(default_job_hist_sizes));
   JobsCompletedSizes.set_levels(default_job_hist_sizes, COUNTOF(default_job_hist_sizes));
   JobsBadputSizes.set_levels(default_job_hist_sizes, COUNTOF(default_job_hist_sizes));

   JobsRunningRuntimes.set_levels(default_job_hist_lifes, COUNTOF(default_job_hist_lifes));
   JobsCompletedRuntimes.set_levels(default_job_hist_lifes, COUNTOF(default_job_hist_lifes));
   JobsBadputRuntimes.set_levels(default_job_hist_lifes, COUNTOF(default_job_hist_lifes));

   int if_poolbasic = (base_verbosity>IF_BASICPUB) ? IF_VERBOSEPUB : IF_BASICPUB;
   int if_poolverbose = (base_verbosity>IF_BASICPUB) ? IF_HYPERPUB : IF_BASICPUB;

   // insert static items into the stats pool so we can use the pool 
   // to Advance and Clear.  these items also publish the overall value
   SCHEDD_STATS_ADD_RECENT(Pool, JobsStarted,          if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExited,           if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCompleted,        if_poolbasic);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumTimeToStart, if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumBadputTime,  if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumExceptionalBadputTime,  if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumRunningTime, if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumExecuteTime, if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumPreExecuteTime,  if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumPostExecuteTime,  if_poolbasic);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumChurnTime,      if_poolverbose);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumExecuteAltTime, if_poolverbose);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsWierdTimestamps,     if_poolverbose);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitedNormally,        if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsKilled,                if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitException,         if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExecFailed,            if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCheckpointed,          if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShadowNoMemory,        if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldRequeue,         if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsNotStarted,            if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldHold,            if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldRemove,          if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCoredumped,            if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsMissedDeferralTime,    if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitedAndClaimClosing, if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsDebugLogError,         if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCoolDown,              if_poolbasic | IF_NONZERO);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsCompletedSizes,        if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsBadputSizes,           if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCompletedRuntimes,     if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsBadputRuntimes,        if_poolbasic);

   SCHEDD_STATS_ADD_VAL(Pool, JobsRunning,                  if_poolbasic);
   SCHEDD_STATS_ADD_VAL(Pool, JobsRunningSizes,             if_poolbasic);
   SCHEDD_STATS_ADD_VAL(Pool, JobsRunningRuntimes,          if_poolbasic);
   SCHEDD_STATS_ADD_VAL(Pool, JobsUnmaterialized,           if_poolbasic);

   //SCHEDD_STATS_PUB_DEBUG(Pool, JobsStarted,  IF_BASICPUB);
   //SCHEDD_STATS_PUB_DEBUG(Pool, JobsCompleted,  IF_BASICPUB);
   //SCHEDD_STATS_PUB_DEBUG(Pool, JobsCompletedSizes,  IF_BASICPUB);
}

   // publish primary statistics (!fOtherPool) at BASIC (verbosity 1)
   // but publish OtherPool (BY_nnn and FOR_xxx) sets at verbosity 2 or 3.
   // Prior to 8.1.2 we didn't pay any attention to the fOtherPool
   // flag when setting the verbosity of the stat, but this resulted in
   // too many verbosity 1 stats and the schedd would take too long to
   // update the collector. So for 8.1.2 we flattened the verbosity levels
   // for the main stats so that the otherpool stats could all be at a higher level.

void ScheddJobStatistics::InitOther(int window, int quantum)
{
   Pool.Clear();
   InitJobCounters(Pool, IF_VERBOSEPUB);
   Pool.SetRecentMax(window, MAX(quantum, 1));
}

void ScheddJobStatistics::SetWindowSize(int window, int quantum)
{
   Pool.SetRecentMax(window, MAX(quantum,1));
}

// extern a schedd_runtime_probe and then add that into the given stats pool.
#define SCHEDD_STATS_ADD_EXTERN_RUNTIME(pool, name, ifpub) extern schedd_runtime_probe name##_runtime;\
	(pool).AddProbe("SC" # name, &name##_runtime, "SC" #name, ifpub | IF_RT_SUM)

//
//
void ScheddStatistics::InitMain()
{
   Clear();
   // default window size to 1 quantum, we may set it to something else later.
   if ( ! this->RecentWindowQuantum) this->RecentWindowQuantum = 1;
   this->RecentWindowMax = this->RecentWindowQuantum;

   InitJobCounters(Pool, IF_BASICPUB);

   JobsRestartReconnectsBadput.set_levels(default_job_hist_lifes, COUNTOF(default_job_hist_lifes));

   SCHEDD_STATS_ADD_RECENT(Pool, JobsSubmitted,        IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, Autoclusters,         IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, ResourceRequestsSent,      IF_BASICPUB);

   SCHEDD_STATS_ADD_RECENT(Pool, ShadowsStarted,            IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, ShadowsRecycled,           IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, ShadowsReconnections,      IF_VERBOSEPUB);

   SCHEDD_STATS_ADD_VAL(Pool, ShadowsRunning,               IF_BASICPUB);
   SCHEDD_STATS_PUB_PEAK(Pool, ShadowsRunning,              IF_BASICPUB);

   SCHEDD_STATS_ADD_VAL(Pool, JobsRestartReconnectsFailed, IF_BASICPUB);
   SCHEDD_STATS_ADD_VAL(Pool, JobsRestartReconnectsLeaseExpired, IF_BASICPUB);
   SCHEDD_STATS_ADD_VAL(Pool, JobsRestartReconnectsSucceeded, IF_BASICPUB);
   SCHEDD_STATS_ADD_VAL(Pool, JobsRestartReconnectsAttempting, IF_BASICPUB);
   SCHEDD_STATS_ADD_VAL(Pool, JobsRestartReconnectsInterrupted, IF_BASICPUB);
   SCHEDD_STATS_ADD_VAL(Pool, JobsRestartReconnectsBadput, IF_BASICPUB);

   // SCHEDD runtime stats for various expensive processes
   //
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, BuildPrioRec,       IF_VERBOSEPUB);
   //SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, BuildPrioRec_mark,  IF_VERBOSEPUB);
   //SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, BuildPrioRec_walk,  IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, BuildPrioRec_sort,  IF_VERBOSEPUB);
   //SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, BuildPrioRec_sweep, IF_VERBOSEPUB);

   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ, IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ_check_for_spool_zombies, IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ_count_a_job,             IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ_PeriodicExprEval,        IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ_clear_autocluster_id,    IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ_add_runnable_local_jobs, IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ_fixAttrUser,             IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ_updateSchedDInterval,    IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ_mark_idle,               IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, WalkJobQ_get_job_prio,            IF_VERBOSEPUB);

   // timings for the autocluster code
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, GetAutoCluster,           IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, GetAutoCluster_hit,       IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, GetAutoCluster_signature, IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_EXTERN_RUNTIME(Pool, GetAutoCluster_cchit,     IF_VERBOSEPUB);
   extern stats_entry_abs<int> SCGetAutoClusterType;
   SCHEDD_STATS_ADD_VAL(Pool, SCGetAutoClusterType, IF_VERBOSEPUB);

   //SCHEDD_STATS_PUB_DEBUG(Pool, JobsSubmitted,  IF_BASICPUB);
}

void ScheddStatistics::Clear()
{
   this->InitTime = time(nullptr);
   this->StatsLifetime = 0;
   this->StatsLastUpdateTime = 0;
   this->RecentStatsTickTime = 0;
   this->RecentStatsLifetime = 0;
   Pool.Clear();
}

// call this when time may have changed to update StatsUpdateTime, etc.
//
// when the time since last update exceeds the time quantum, then we Advance the ring
// buffers so that we throw away the oldest value and begin accumulating the latest
// value in a new slot. 
//
time_t ScheddStatistics::Tick(time_t now)
{
   if ( ! now) now = time(nullptr);

   int cAdvance = generic_stats_Tick(
      now,
      this->RecentWindowMax,   // RecentMaxTime
      this->RecentWindowQuantum, // RecentQuantum
      this->InitTime,
      this->StatsLastUpdateTime,
      this->RecentStatsTickTime,
      this->StatsLifetime,
      this->RecentStatsLifetime);

   if (cAdvance)
      Pool.Advance(cAdvance);

   AdvanceAtLastTick = cAdvance;
   return now;
}

void ScheddStatistics::Publish(ClassAd & ad) const
{
   this->Publish(ad, this->PublishFlags);
}

void ScheddStatistics::Publish(ClassAd & ad, const char * config) const
{
   int flags = this->PublishFlags;
   if (config && config[0]) {
      flags = generic_stats_ParseConfigString(config, "SCHEDD", "SCHEDULER", IF_BASICPUB | IF_RECENTPUB);
   }
   this->Publish(ad, flags);
}

void ScheddStatistics::Publish(ClassAd & ad, int flags) const
{
   if ((flags & IF_PUBLEVEL) > 0) {
      ad.Assign("StatsLifetime", StatsLifetime);
      ad.Assign("JobsSizesHistogramBuckets", default_sizes_set);
      ad.Assign("JobsRuntimesHistogramBuckets", default_lifes_set);
      if (flags & IF_VERBOSEPUB)
         ad.Assign("StatsLastUpdateTime", StatsLastUpdateTime);
      if (flags & IF_RECENTPUB) {
         ad.Assign("RecentStatsLifetime", RecentStatsLifetime);
         if (flags & IF_VERBOSEPUB) {
            ad.Assign("RecentWindowMax", RecentWindowMax);
            ad.Assign("RecentStatsTickTime", RecentStatsTickTime);
         }
      }
   }
   Pool.Publish(ad, flags);
}

#if 0  // obsolete
void ScheddStatistics::Unpublish(ClassAd & ad) const
{
   ad.Delete("StatsLifetime");
   ad.Delete("StatsLastUpdateTime");
   ad.Delete("RecentStatsLifetime");
   ad.Delete("RecentStatsTickTime");
   ad.Delete("RecentWindowMax");
   Pool.Unpublish(ad);
}
#endif

ScheddOtherStats::ScheddOtherStats()
	: next(nullptr)
	, trigger_expr(nullptr)
	, enabled(false)
	, by(false)
	, last_match_time(0)
	, lifetime(0)
{
}

ScheddOtherStats::~ScheddOtherStats()
{
	delete trigger_expr; trigger_expr = nullptr;
	for (auto & set : sets) {
		delete set.second;
		set.second = NULL;
	}
	sets.clear();
}

ScheddOtherStatsMgr::~ScheddOtherStatsMgr()
{
	Clear();
}

void ScheddOtherStatsMgr::Clear()
{
	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {
		delete po;
	}
	pools.clear();
}

time_t ScheddOtherStatsMgr::Tick(time_t now) // call this when time may have changed to update StatsUpdateTime, etc.
{
	ASSERT(now == config.StatsLastUpdateTime);
	int cAdvance = config.AdvanceAtLastTick;

	// update deferred counts of submitted jobs by/for expression
	CountJobsSubmitted();

	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {
		if (cAdvance) { po->stats.Advance(cAdvance); }
		if ( ! po->sets.empty()) {
			for (auto & set : po->sets) {
				ScheddOtherStats* po2 = set.second;
				// if the statistics set has expired, disable it 
				// otherwise, call its tick method.
				if ((po2->lifetime > 0) && (po2->last_match_time > 0) &&
					(po2->last_match_time < now) && 
					((po2->last_match_time + po2->lifetime) < now)) {
					po2->enabled = false;
				} else {
					if (cAdvance) { po2->stats.Advance(cAdvance); }
				}
			}
		}
	}
	return now;
}

void ScheddOtherStatsMgr::Reconfig(int window, int quantum)
{
	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {
		po->stats.SetWindowSize(window, quantum);
		if ( ! po->sets.empty()) {
			for (auto & set : po->sets) {
				// don't really want to re-parse the same config knob for each child
				// so instead, just copy the flags and windows max from the base set.
				// and resize the recent window arrays if needed.
				//
				ScheddOtherStats* po2 = set.second;
				po2->stats.SetWindowSize(window, quantum);
			}
		}
	}
}

#if 0
void ScheddOtherStatsMgr::Publish(ClassAd & ad)
{
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {

		if ( ! po->enabled)
			continue;

		po->stats.Pool.Publish(ad, po->prefix.c_str(), po->stats.PublishFlags);
		if ( ! po->sets.empty()) {
			for (std::map<std::string, ScheddOtherStats*>::iterator it = po->sets.begin();
				 it != po->sets.end();
				 ++it) {

				ScheddOtherStats* po2 = it->second;
				if (po2->enabled) {
					po2->stats.Pool.Publish(ad, po2->prefix.c_str(), po->stats.PublishFlags);
				}

			}
		}
	}
}
#endif

void ScheddOtherStatsMgr::Publish(ClassAd & ad, int flags)
{
	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {

		if ( ! po->enabled)
			continue;

		po->stats.Pool.Publish(ad, po->prefix.c_str(), flags);
		if ( ! po->sets.empty()) {
			for (auto & set : po->sets) {

				ScheddOtherStats* po2 = set.second;
				if (po2->enabled) {
					po2->stats.Pool.Publish(ad, po2->prefix.c_str(), flags);
				}

			}
		}
	}
}

void ScheddOtherStatsMgr::UnpublishDisabled(ClassAd & ad)
{
	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {

		if ( ! po->enabled) {
			po->stats.Pool.Unpublish(ad);
		}

		if ( ! po->sets.empty()) {
			for (auto it = po->sets.begin();
				 it != po->sets.end();
				 ++it) {

				ScheddOtherStats* po2 = it->second;
				if ( ! po2->enabled || ! po->enabled) {
					po2->stats.Pool.Unpublish(ad, po2->prefix.c_str());
				}

			}
		}
	}

}


// keep track of jobs that have been submitted but not yet counted
void ScheddOtherStatsMgr::DeferJobsSubmitted(int cluster, int proc)
{
	// keep track of the last proc seen for each cluster 
	// since we processed (and cleared) the deferred_jobs set
	if (pools.getNumElements() > 0) {
		deferred_jobs_submitted[cluster] = proc;
	}
}

// finish deferred counting of submitted jobs.
void ScheddOtherStatsMgr::CountJobsSubmitted()
{
	if ( ! deferred_jobs_submitted.empty() &&
		pools.getNumElements() > 0) {
		time_t now = time(nullptr);

		for (auto & it : deferred_jobs_submitted) {
			int cluster = it.first;
			int last_proc = it.second;
			for (int proc = 0; proc <= last_proc; ++proc) {
				ClassAd * job_ad = GetJobAd_as_ClassAd(cluster, proc);
				if (job_ad) {
					ScheddOtherStats *po = Matches(*job_ad, now);
					while (po) {
						po->stats.JobsSubmitted += 1;
						po = po->next;
					}
					FreeJobAd(job_ad);
				}
			}
		}
	}
	deferred_jobs_submitted.clear();
}

// reset jobs-running counters/histograms in preparation for count_jobs
void ScheddOtherStatsMgr::ResetJobsRunning()
{
	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {

		// don't bother to reset disabled stats.
		if ( ! po->enabled)
			continue;

		po->stats.JobsRunning = 0;
		po->stats.JobsRunningRuntimes = 0;
		po->stats.JobsRunningSizes = 0;
		po->stats.JobsUnmaterialized = 0;

		if (po->sets.empty())
			continue;

		for (auto & set : po->sets) {

			ScheddOtherStats* po2 = set.second;
			if (po2->enabled) {
				po2->stats.JobsRunning = 0;
				po2->stats.JobsRunningRuntimes = 0;
				po2->stats.JobsRunningSizes = 0;
				po2->stats.JobsUnmaterialized = 0;
			}
		}
	}
}



/* don't use these.
void ScheddOtherStatsMgr::SetWindowSize(int window)
{
}

void Publish(ClassAd & ad, const char * config) const
{
}
*/

bool ScheddOtherStatsMgr::Enable(
	const char * pre, 
	const char * trig, 
	bool stats_by_trig_value, /*=false*/
	time_t lifetime /*=0*/)
{
	ExprTree *tree = nullptr;
	classad::ClassAdParser  parser;
	if ( ! parser.ParseExpression(trig, tree)) {
		EXCEPT("Schedd_stats: Invalid trigger expression for '%s' stats: '%s'", pre, trig);
	}

	bool was_enabled = false;
	ScheddOtherStats* po = nullptr;
	if (pools.lookup(pre, po) < 0) {
		po = new ScheddOtherStats();
		ASSERT(po);
		po->prefix = pre;
		po->stats.InitOther(config.RecentWindowMax, (int)config.RecentStatsLifetime);
		pools.insert(pre, po);
	} else {
		was_enabled = po->enabled;
		if (po->by != stats_by_trig_value ||
			po->trigger != trig) {
			po->stats.Clear();
		}
		delete po->trigger_expr; po->trigger_expr = nullptr;
	}

	po->enabled = true;
	po->by = stats_by_trig_value;
	po->trigger = trig; // update trigger.
	po->trigger_expr = tree;
	po->lifetime = lifetime;
	return was_enabled;
}

bool ScheddOtherStatsMgr::DisableAll()
{
	bool any_enabled = false;
	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {
		any_enabled = any_enabled || po->enabled;
		po->enabled = false;
	}
	return any_enabled;
}

bool ScheddOtherStatsMgr::RemoveDisabled()
{
	bool any_removed = false;
	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {
		if ( ! po->enabled) {
			std::string key;
			pools.getCurrentKey(key);
			pools.remove(key);
			delete po;
			any_removed = true;
		} else if ( ! po->sets.empty()) {
			for (auto it = po->sets.begin();
				 it != po->sets.end();
				 ) {
				if ( ! po->enabled) {
					delete it->second;
					it->second = NULL;
					po->sets.erase(it++);
				} else {
					it++;
				}
			}
		}
	}
	return any_removed;
}

bool ScheddOtherStatsMgr::AnyEnabled()
{
	if (pools.getNumElements() <= 0)
		return false;
	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {
		if (po->enabled)
			return true;
	}
	return false;
}


// returns a linked list of matches, the caller does not free
// the returned list, the returned list is only valid until
// the next time a method on this class is called.
ScheddOtherStats * ScheddOtherStatsMgr::Matches(ClassAd & ad, time_t updateTime)
{
	ScheddOtherStats* poLast = nullptr;
	ScheddOtherStats* po = nullptr;
	pools.startIterations();
	while (pools.iterate(po)) {

		// if we have not yet built a parse tree for this expression, do that now.
		if ( ! po->trigger_expr) {
			ExprTree *tree = nullptr;
			classad::ClassAdParser  parser;
			if ( ! parser.ParseExpression(po->trigger, tree)) {
				dprintf(D_ALWAYS, "Schedd_stats: Failed to parse expression for %s stats: '%s'\n", 
						po->prefix.c_str(), po->trigger.c_str());
				continue;
			}
			po->trigger_expr = tree;
		}

		classad::Value val;
		if ( ! ad.EvaluateExpr(po->trigger_expr, val)) 
			continue;

		// check to see if this is a _FOR_XXX type or _BY_XXX type. 
		// if the expression evaluates to a bool, then count it
		// in the overall stats collection (like it was a _FOR_XXX type)
		// even if it's a _BY_XXX type.
		//
		if ( ! po->by || val.IsBooleanValue()) {
			bool bb = false;
			if (val.IsBooleanValueEquiv(bb) && bb) {
				po->last_match_time = updateTime;
				po->next = poLast;
				poLast = po;
			}
			continue;
		} 

		// If we get here, this is a _BY_xxx type stats pool. 
		// for this type we want to separate set of counters for each
		// unique value that the trigger expression evaluates to
		//
		if (val.IsUndefinedValue()) {
			continue;
		}

		// convert the expression result to a string so we 
		// can use it to do a lookup in the po->sets map
		//
		std::string str;
		if (val.IsStringValue(str)) {
			// str already has value.
		} else if (val.IsErrorValue()) {
			str = "ERROR"; // so that errors become visible
		} else {
			classad::Value strVal;
			if ( ! convertValueToStringValue(val, strVal)) {
				// ignore things that can't be converted to strings
				continue;
			}
			strVal.IsStringValue(str);
		}

		// treat empty strings as not matching anything.
		//
		if (str.empty())
			continue;

		// for non-empty strings, we want to add the current
		// statistics set, and the set at po->sets[str];
		// we may need to create the set at po->sets[str] if 
		// this is the first time we have seen this value of str

		// add the current stats pool to the return list
		// this will accumulate and publish the attributes
		// that will contain totals for all counters in the stats set for "Prefix"
		po->last_match_time = updateTime;
		po->next = poLast;
		poLast = po;

		// locate or create a stats set for the key str
		ScheddOtherStats* po2 = nullptr;
		auto it = po->sets.find(str);
		if (it == po->sets.end()) {
			po2 = new ScheddOtherStats();
			ASSERT(po2);
			po->sets[str] = po2;

			formatstr(po2->prefix, "%s_%s_", po->prefix.c_str(), str.c_str());
			cleanStringForUseAsAttr(po2->prefix, '_', false);

			po2->stats.InitOther(config.RecentWindowMax, config.RecentWindowQuantum);
			po2->lifetime = po->lifetime;

			po2->enabled = true;
			po2->by = false;
			po2->next = nullptr;
			po2->trigger_expr = nullptr;
		} else {
			po2 = it->second;
		}

		// add the by<str> stats to the return list.
		// This will accumulate and publish the "Prefix_str_JobsEtc" attributes
		if (po2) {
			po2->last_match_time = updateTime;
			po2->next = poLast;
			poLast = po2;
		}

	} // end while

	// return the linked list of matches
	return poLast;
}


//#define UNIT_TEST
#ifdef UNIT_TEST
void schedd_stats_unit_test (ClassAd * pad)
{
   ScheddStatistics stats;
   stats.Init();

   int stat_window = 300; //param_integer("STATISTICS_WINDOW_SECONDS", 1200, 1, INT_MAX);
   stats.SetWindowSize(stat_window);

   stats.Tick();
   stats.JobsStarted += 1;
   stats.JobsCompleted += 1;
   stats.JobSizes += 99;

   stats.ShadowsRunning = 32;

   stats.Tick();

   stats.ShadowsRunning = 30;
   stats.JobsStarted += 1;
   stats.JobsCompleted += 1;

   if (pad) {
      stats.Publish(*pad);
   }
}
#endif
